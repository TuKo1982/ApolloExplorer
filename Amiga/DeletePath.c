#include "DeletePath.h"

#include <string.h>
#include <dos/dos.h>
#include <clib/dos_protos.h>
#include <clib/exec_protos.h>

#define DBGOUT 0
#include "AEUtil.h"
#include "protocolTypes.h"
#define BNULL 0


#define DELETE_MSG_SIZE ( sizeof( ProtocolMessage_PathDeleted_t ) + (MAX_FILEPATH_LENGTH*3) )   //We allocate enough space for unexpectedly long paths
#define DELETE_ANCHOR_SIZE ( sizeof( struct AnchorPath ) + (MAX_FILEPATH_LENGTH*3) )

DeleteContext_t *allocateDeleteContext()
{
    DeleteContext_t *context = AllocVec( sizeof( DeleteContext_t ), MEMF_FAST|MEMF_CLEAR );
    if( context == NULL ) return NULL;

    context->pathDelMsg = AllocVec( DELETE_MSG_SIZE, MEMF_FAST|MEMF_CLEAR );
    context->anchorPath = AllocVec( DELETE_ANCHOR_SIZE, MEMF_FAST|MEMF_CLEAR );
    if( context->pathDelMsg == NULL || context->anchorPath == NULL )
    {
        if( context->pathDelMsg != NULL ) FreeVec( context->pathDelMsg );
        if( context->anchorPath != NULL ) FreeVec( context->anchorPath );
        FreeVec( context );
        return NULL;
    }

    //Set up the message
    context->pathDelMsg->header.type = PMT_PATH_DELETED;
    context->pathDelMsg->header.length = sizeof( *context->pathDelMsg );  //This needs to be overwritten on a per-path basis
    context->pathDelMsg->header.token = MAGIC_TOKEN;
    return context;
}

void freeDeleteContext( DeleteContext_t *context )
{
    if( context == NULL ) return;
    endRecursiveDelete( context );
    FreeVec( context->pathDelMsg );
    FreeVec( context->anchorPath );
    FreeVec( context );
}

static void initialiseDelete( DeleteContext_t *context )
{
    //Just to be on the safe side, clear out the memory structs
    memset( context->pathDelMsg, 0, DELETE_MSG_SIZE );
    memset( context->anchorPath, 0, DELETE_ANCHOR_SIZE );
    memset( context->currentPath, 0, sizeof( context->currentPath ) );
    context->anchorPath->ap_Strlen = MAX_FILEPATH_LENGTH;
}

BOOL inline isDeletable( struct	FileInfoBlock *fib )
{
	if( fib->fib_Protection & FIBF_DELETE )
		return FALSE;
	return TRUE;
}


ProtocolMessage_PathDeleted_t *deleteFile( char *path, DeleteContext_t *context )
{
	dbglog( "[deleteFile] Deleting file '%s'\n", path );
    //First check that the string is valid
    if( path == NULL || ( strlen( path ) > MAX_FILEPATH_LENGTH ) )
    {
        //Well we are already carrying out an operation
        context->pathDelMsg->deleteSucceeded = 0;
        context->pathDelMsg->failureReason = DFR_PATH_NOT_FOUND;
        context->pathDelMsg->deleteCompleted = 0;
        context->pathDelMsg->filePath[ 0 ] = 0;
        context->pathDelMsg->header.length = sizeof( *context->pathDelMsg );
        context->pathDelMsg->header.type = PMT_PATH_DELETED;
        context->pathDelMsg->header.token = MAGIC_TOKEN;
        return context->pathDelMsg;
    }

    //First, check that we aren't in a current operation
    if( strlen( context->currentPath ) )
    {
        //Well we are already carrying out an operation
        context->pathDelMsg->deleteSucceeded = 0;
        context->pathDelMsg->failureReason = DFR_OPERATION_ALREADY_UNDERWAY;
        context->pathDelMsg->deleteCompleted = 0;
        context->pathDelMsg->header.type = PMT_PATH_DELETED;
        context->pathDelMsg->header.token = MAGIC_TOKEN;
        context->pathDelMsg->filePath[ 0 ] = 0;
        context->pathDelMsg->header.length = sizeof( *context->pathDelMsg );
        return context->pathDelMsg;
    }

    //Initialise the delete (if we didn't already)
    initialiseDelete( context );

    //Do the delete
	if( DeleteFile( path ) )
	{
		dbglog( "[deleteFile] Deleted file '%s'\n", path );
		context->pathDelMsg->deleteSucceeded = 1;
		context->pathDelMsg->failureReason = DFR_NONE;
		context->pathDelMsg->deleteCompleted = 1;
		context->pathDelMsg->header.type = PMT_PATH_DELETED;
		context->pathDelMsg->header.token = MAGIC_TOKEN;
		context->pathDelMsg->header.length = sizeof( *context->pathDelMsg );
		context->pathDelMsg->filePath[ 0 ] = 0;
		return context->pathDelMsg;
	}

    //Else we had some sort of missmatch
    context->failureReason = DFR_UNKNOWN;
    return getDeleteError( context );
}

char startRecursiveDelete( char* path, DeleteContext_t *context )
{
    //First check that the string is valid
    if( path == NULL || strlen( path ) > MAX_FILEPATH_LENGTH )
    {
        //Well we are already carrying out an operation
        context->failureReason = DFR_PATH_NOT_FOUND;
        memset( context->currentPath, 0, sizeof( context->currentPath ) );
        dbglog( "[startRecursiveDelete] Path invalid or missing.\n" );
        return 1;
    }

    //First, check that we aren't in a current operation
    if( strlen( context->currentPath ) )
    {
        //Well we are already carrying out an operation
        context->failureReason = DFR_OPERATION_ALREADY_UNDERWAY;
        dbglog( "[startRecursiveDelete] Delete is already underway.\n" );
        return 1;
    }

    //Initialise the deletion
    initialiseDelete( context );
    strncpy( context->currentPath, path, sizeof( context->currentPath ) - 1 );
    context->failureReason = DFR_NONE;

    //Make sure that the string has terminating "/"
    if( 	context->currentPath[ strlen( context->currentPath ) - 1 ] == ':' &&
    		context->currentPath[ strlen( context->currentPath ) - 1 ] == '/')
    {
    	context->currentPath[ strlen( context->currentPath ) ] = '/';
    }

    //Now find the first match
    dbglog( "[startRecursiveDelete] Starting recursive delete for path %s\n", context->currentPath );
    LONG returnCode = MatchFirst( context->currentPath, context->anchorPath );
    if( returnCode > 0 )
    {
    	//TODO: Find all the error scenarios here
    	dbglog( "[startRecursiveDelete] Failed to find the first match for path %s.\n", context->currentPath );
    	context->failureReason = DFR_UNKNOWN;
    	MatchEnd( context->anchorPath );
    	return 1;
    }
    context->matchActive = TRUE;
    context->anchorPath->ap_Flags |= APF_DODIR; //We want to recurse down through this dir

    return 0;
}

ProtocolMessage_PathDeleted_t *getDeleteError( DeleteContext_t *context )
{
    //Form the message based on the last operations
    context->pathDelMsg->deleteSucceeded = context->failureReason > DFR_NONE ? 0 : 1;
    context->pathDelMsg->deleteCompleted = 0;
    context->pathDelMsg->failureReason = context->failureReason;
    context->pathDelMsg->header.type = PMT_PATH_DELETED;
    context->pathDelMsg->header.token = MAGIC_TOKEN;
    context->pathDelMsg->header.length = sizeof( *context->pathDelMsg );
    context->pathDelMsg->filePath[ 0 ] = 0;
    //context->pathDelMsg->header.length = sizeof( *context->pathDelMsg ) + strlen( context->currentPath );
    return context->pathDelMsg;
}

ProtocolMessage_PathDeleted_t *getNextFileDeleted( DeleteContext_t *context )
{
	//Now get the first entry in this directory
	//dbglog( "[getNextFileDeleted] Get the next match\n" );
	LONG returnCode = MatchNext( context->anchorPath );
	if( returnCode > 0 )
	{
		dbglog( "[getNextFileDeleted] Failed to get the next match.  No more entries?\n" );
		context->failureReason = DFR_UNKNOWN;
		memset( context->currentPath, 0, sizeof( context->currentPath ) );
		return NULL;
	}

	//Copy the path to our globals
	strncpy( context->currentPath, context->anchorPath->ap_Buf, MAX_FILEPATH_LENGTH );

	dbglog( "[getNextFileDeleted] Path %s ", context->anchorPath->ap_Buf );
	switch( context->anchorPath->ap_Info.fib_EntryType )
	{
		case DET_USERDIR:
		{
			dbglog( "directory");
			if( context->anchorPath->ap_Flags & APF_DIDDIR  )
			{
				//We want to delete this now, so lock the parent directory
				char dirPath[ MAX_FILEPATH_LENGTH ] __attribute__((aligned(4)));
				strncpy( dirPath, context->anchorPath->ap_Buf, MAX_FILEPATH_LENGTH );
				char *endOfPath = PathPart( dirPath );
				*endOfPath=0;	//null terminate it so we have the path
				if( context->dirLock != BNULL )	UnLock( context->dirLock );
				context->dirLock = Lock( dirPath, ACCESS_WRITE );

				context->anchorPath->ap_Flags &= ~APF_DIDDIR;
				dbglog( " - deleting ");

				//Check if the file is delete protected
				if( !isDeletable( &context->anchorPath->ap_Info ) )
				{
					dbglog( "[getNextFileDeleted] removing delete protection from %s\n", context->anchorPath->ap_Buf );
					SetProtection( context->anchorPath->ap_Buf, 0 );
				}

				if( DeleteFile( context->anchorPath->ap_Buf ) )
				{
					dbglog( " - success ");
					context->pathDelMsg->deleteSucceeded = 1;
					context->pathDelMsg->failureReason = DFR_NONE;
				}
				else
				{
					dbglog( " - failed " );
					context->pathDelMsg->deleteSucceeded = 0;
					context->pathDelMsg->failureReason = DFR_UNKNOWN;
				}
				if( context->dirLock != BNULL )	UnLock( context->dirLock );
				context->dirLock = BNULL;
			}
			else
			{
				dbglog( " - entering ");
				context->anchorPath->ap_Flags |= APF_DODIR;
			}
			break;
		}
		case DET_FILE:
		{
			dbglog( "file" );
			dbglog( " - deleting ");

			//Check if the file is delete protected
			if( !isDeletable( &context->anchorPath->ap_Info ) )
			{
				dbglog( "[getNextFileDeleted] removing delete protection from %s\n", context->anchorPath->ap_Buf );
				SetProtection( context->anchorPath->ap_Buf, 0 );
			}

			if( DeleteFile( context->anchorPath->ap_Buf ) )
			{
				dbglog( " - success " );
				context->pathDelMsg->deleteSucceeded = 1;
				context->pathDelMsg->failureReason = DFR_NONE;
			}
			else
			{
				dbglog( " - failed " );
				context->pathDelMsg->deleteSucceeded = 0;
				context->pathDelMsg->failureReason = DFR_UNKNOWN;
			}
			break;
		}
		default:
			dbglog( "other" );
			break;
	}
	dbglog( "\n" );

	//Form the response message
	//dbglog( "[getNextFileDeleted] completed\n" );
	context->pathDelMsg->deleteCompleted = 0;
    context->pathDelMsg->header.type = PMT_PATH_DELETED;
    context->pathDelMsg->header.token = MAGIC_TOKEN;
	context->pathDelMsg->header.length = sizeof( *context->pathDelMsg ) + strlen( context->currentPath );
	strncpy( context->pathDelMsg->filePath, context->currentPath, MAX_FILEPATH_LENGTH );
	return context->pathDelMsg;
}

char endRecursiveDelete( DeleteContext_t *context )
{
	//Only end a match that was started and not yet ended: the caller also calls this
	//after a failed start, where MatchFirst() already got its MatchEnd().
	if( context->matchActive )
	{
		MatchEnd( context->anchorPath );
		context->matchActive = FALSE;
	}
	memset( context->currentPath, 0, sizeof( context->currentPath ) );
	if( context->dirLock != BNULL ) UnLock( context->dirLock );
	context->dirLock = BNULL;
	return 0;
}


ProtocolMessage_PathDeleted_t *getRecusiveDeleteCompletedMessage( DeleteContext_t *context )
{
	context->pathDelMsg->deleteSucceeded = 1;
	context->pathDelMsg->deleteCompleted = 1;
	context->pathDelMsg->failureReason = DFR_NONE;
	context->pathDelMsg->filePath[0] = '\0';
    context->pathDelMsg->header.type = PMT_PATH_DELETED;
    context->pathDelMsg->header.token = MAGIC_TOKEN;
	context->pathDelMsg->header.length = sizeof( *context->pathDelMsg );
	return context->pathDelMsg;
}
