/*
 * ReceiveFile.h
 *
 *  Created on: May 23, 2021
 *      Author: rony
 */

#ifndef AMIGA_RECEIVEFILE_H_
#define AMIGA_RECEIVEFILE_H_

#include "protocolTypes.h"
#include <proto/dos.h>
#include <proto/exec.h>

//Per-connection state of a file upload (client -> Amiga).  Each client thread owns
//one, so concurrent uploads can't close or write each other's files.
typedef struct
{
	BPTR fileHandle;
	ULONG currentChunk;
	ULONG totalChunks;
	ULONG fileSize;
	unsigned int bytesWrittenToFile;
	char filePath[ MAX_FILEPATH_LENGTH ];

	//Protocol messages we will use
	ProtocolMessage_Ack_t *acknowledgeMessage;
} FileReceiveContext_t;

ProtocolMessage_Ack_t *requestFileReceive( char *path, FileReceiveContext_t *context );

void putStartOfFileReceive( ProtocolMessage_StartOfFileSend_t *startOfFilesendMessage, FileReceiveContext_t *context );

int putNextFileSendChunk( ProtocolMessage_FileChunk_t *fileChunkMessage, FileReceiveContext_t *context );

unsigned int getBytesWritenToFile( FileReceiveContext_t *context );

void cleanupFileReceive( FileReceiveContext_t *context );

FileReceiveContext_t *allocateFileReceiveContext();

void freeFileReceiveContext( FileReceiveContext_t *context );

#endif /* AMIGA_RECEIVEFILE_H_ */
