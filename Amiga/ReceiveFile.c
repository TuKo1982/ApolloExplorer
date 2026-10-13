/*
 * ReceiveFile.c
 *
 *  Created on: May 23, 2021
 *      Author: rony
 */

#include "ReceiveFile.h"

#define DBGOUT 0

#ifdef __GNUC__
#if DBGOUT
#include <stdio.h>
#endif
#include <string.h>
#include <proto/dos.h>

#include <proto/exec.h>
#endif

#ifdef __VBCC__

#endif

#include "AEUtil.h"
#include "protocol.h"
#include "protocolTypes.h"

ProtocolMessage_Ack_t *requestFileReceive( char *path, FileReceiveContext_t *context )
{
	dbglog( "[requestFileReceive] We've been asked to recieve file '%s'\n", path );
	context->bytesWrittenToFile = 0;

	//First, let's try and open a file for writing at the requested location
	dbglog( "[requestFileReceive] Attempting to open file '%s'\n", path );
	context->fileHandle = Open( path, MODE_NEWFILE );
	strncpy( context->filePath, path, sizeof( context->filePath ) - 1 );

	//Did we succeed?
	if( context->fileHandle )
	{
		dbglog( "[requestFileReceive] File '%s' could be opened\n", path );
		context->acknowledgeMessage->response = 1;
	}
	else
	{
		dbglog( "[requestFileReceive] File '%s' could NOT be opened\n", path );

		//Let's dig deeper and see if we can see why this is the case
		char returnCode = AT_NOK;
		BPTR dirLock = Lock( path, ACCESS_READ );
		if( dirLock )
		{
			//OK this path already exists.   But is it a directory?
			struct FileInfoBlock fileInfoBlock;
			dbglog( "[requestFileReceive] Path %s alread exists\n", path );
			if( Examine( dirLock, &fileInfoBlock ) )
			{
				dbglog( "makeDir() Examining %s\n", path );
				if( fileInfoBlock.fib_DirEntryType > 0 )
				{
					dbglog( "makeDir() Path %s is already a file.  We can't make a directory here.\n", path );
					returnCode = AT_DEST_EXISTS_AS_DIR;
				}
			}
			UnLock( dirLock );

		}
		context->acknowledgeMessage->response = returnCode;
	}

	return context->acknowledgeMessage;
}

void putStartOfFileReceive( ProtocolMessage_StartOfFileSend_t *startOfFilesendMessage, FileReceiveContext_t *context )
{
	//Now we get the file information from the sender
	context->currentChunk = 0;
	context->totalChunks = startOfFilesendMessage->numberOfFileChunks;
	context->fileSize = startOfFilesendMessage->fileSize;

	dbglog( "[putStartOfFileReceive] The file contains %lu chunks and is %lu bytes in size.\n", context->totalChunks, context->fileSize );
}

int putNextFileSendChunk( ProtocolMessage_FileChunk_t *fileChunkMessage, FileReceiveContext_t *context )
{
	dbglog( "[putNextFileSendChunk] filechunk number: %d bytesContained: %d\n", fileChunkMessage->chunkNumber, fileChunkMessage->bytesContained );

	//Now check that we really have bytes to write
	if( fileChunkMessage->bytesContained == 0 )
	{
		//If we are not finished writting, something went wrong at the sender's end
		dbglog( "[putNextFileSendChunk] Empty file being sent?  File: '%s'.  Aborting.\n", context->filePath );
		cleanupFileReceive( context );
		return 0;
	}

	//Book keeping
	context->currentChunk = fileChunkMessage->chunkNumber;

	//Write the contents to the disk
	LONG bytesWriten __attribute__((aligned(4))) = 0;
	LONG totalBytesWriten __attribute__((aligned(4))) = 0;
	LONG bytesRemaining __attribute__((aligned(4))) = fileChunkMessage->bytesContained;
	while( bytesRemaining > 0 )
	{
		bytesWriten = Write( context->fileHandle, fileChunkMessage->chunk + totalBytesWriten, bytesRemaining );
		if( bytesWriten <= 0 )
		{
			//Write error (disk full, write protected, ...).  Stop instead of looping
			//forever; the caller reports the bytes written so far to the client.
			dbglog( "[putNextFileSendChunk] Write to '%s' failed (IoErr %ld).  Aborting.\n", context->filePath, IoErr() );
			return 0;
		}
		totalBytesWriten += bytesWriten;
		bytesRemaining -= bytesWriten;
		context->bytesWrittenToFile += bytesWriten;
		dbglog( "[putNextFileSendChunk] Wrote %ld bytes to file.\r", totalBytesWriten );
	}
	dbglog( "\n" );

	//If this is the last chunk, close the file
	if( context->currentChunk == ( context->totalChunks - 1 ) )
	{
		dbglog( "[putNextFileSendChunk] File '%s' now complete.  Closing.\n", context->filePath );
		//cleanupFileReceive();
		return 0;
	}

	//We should return the number of chunks we are still expecting
	dbglog( "[putNextFileSendChunk] Current chunk %lu.  Total chunks %lu.  Chunks remaining %lu\n", context->currentChunk, context->totalChunks, context->totalChunks - context->currentChunk -1 );
	return context->totalChunks - context->currentChunk -1;
}

unsigned int getBytesWritenToFile( FileReceiveContext_t *context )
{
	return context->bytesWrittenToFile;
}

void cleanupFileReceive( FileReceiveContext_t *context )
{
	//Valid context?
	if( context == NULL ) return;

	dbglog( "[cleanupFileReceive] Cleaning up for file '%s'.\n", context->filePath );

	//Close the file if it is open
	if( context->fileHandle )
	{
		Close( context->fileHandle );
		context->fileHandle = (BPTR)NULL;
	}

	//Reset book keeping
	context->fileSize = 0;
	context->currentChunk = 0;
	context->totalChunks = 0;
	memset( context->filePath, 0, sizeof( context->filePath ) );
}

FileReceiveContext_t *allocateFileReceiveContext()
{
	FileReceiveContext_t *context = (FileReceiveContext_t*)AllocVec( sizeof( FileReceiveContext_t ), MEMF_CLEAR|MEMF_FAST );
	if( context == NULL ) return NULL;

	//Allocate the acknowledge message
	context->acknowledgeMessage = AllocVec( sizeof( ProtocolMessage_Ack_t ), MEMF_FAST|MEMF_CLEAR );
	if( context->acknowledgeMessage == NULL )
	{
		FreeVec( context );
		return NULL;
	}
	context->acknowledgeMessage->header.token = MAGIC_TOKEN;
	context->acknowledgeMessage->header.length = sizeof( ProtocolMessage_Ack_t );
	context->acknowledgeMessage->header.type = PMT_ACK;

	return context;
}

void freeFileReceiveContext( FileReceiveContext_t *context )
{
	//Is it a valid pointer?
	if( context == NULL )	return;

	//Make sure we have cleaned up after ourselves
	cleanupFileReceive( context );

	//Free everything
	FreeVec( context->acknowledgeMessage );
	FreeVec( context );
}
