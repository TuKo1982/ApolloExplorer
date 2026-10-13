/*
 * DeletePath.h
 *
 *  Created on: August, 2022
 *      Author: rony
 */

#ifndef AMIGA_DELETEPATH_H_
#define AMIGA_DELETEPATH_H_

#include "protocolTypes.h"
#include <dos/dos.h>
#include <dos/dosasl.h>

/**
 * @brief Per-connection state of a delete.  Each client thread owns one, so concurrent
 * deletes from different clients don't share (and MatchEnd/free) each other's anchor.
 */
typedef struct
{
    ProtocolMessage_PathDeleted_t *pathDelMsg;
    DeleteFailureReason failureReason;
    char currentPath[ MAX_FILEPATH_LENGTH * 2 ];    //If there is an operation underway, this will be populated.
    struct AnchorPath *anchorPath;
    BOOL matchActive;                               //TRUE between a successful MatchFirst() and its MatchEnd()
    BPTR dirLock;
} DeleteContext_t;

/**
 * @brief Delete a single file
 * 
 * @param path The full path to the file to be deleted
 * @return ProtocolMessage_PathDeleted_t* A response containing either a code indicating success or a code indicating failure and which failure should the file not be deleted.
 */
ProtocolMessage_PathDeleted_t *deleteFile( char *path, DeleteContext_t *context );

/**
 * @brief Start a recursive deletion of the directory and all files and subdirectories.  The server will go down through the directory
 * structure and delete all files and directories that it finds.
 * 
 * @param path The path to the toplevel of the path (i.e. the directory) which is to be deleted.
 * @return true If this path can be locked and the recursive delete started.
 * @return false If the delete cannot be started.
 */
char startRecursiveDelete( char* path, DeleteContext_t *context );

/**
 * @brief Get a message containing the error of the last operation
 * @return ProtocolMessage_PathDeleted_t* 
 */
ProtocolMessage_PathDeleted_t *getDeleteError( DeleteContext_t *context );

/**
 * @brief Get the next file which was deleted.
 * @return ProtocolMessage_PathDeleted_t* 
 */
ProtocolMessage_PathDeleted_t *getNextFileDeleted( DeleteContext_t *context );

/**
 * @brief End the last recursive delete operation OR interrupt a currently running one.
 * 
 * @return true If there is a current recursive operation
 * @return false In all other cases
 */
char endRecursiveDelete( DeleteContext_t *context );

/**
 * @brief Get a message to indicate the deletion of the message has been compelted.
 */
ProtocolMessage_PathDeleted_t *getRecusiveDeleteCompletedMessage( DeleteContext_t *context );

/**
 * @brief Allocate the delete state for one client thread.
 */
DeleteContext_t *allocateDeleteContext();

/**
 * @brief End any running delete and free the context.
 */
void freeDeleteContext( DeleteContext_t *context );


#endif /* AMIGA_AMIGA_DELETEPATH_H_ */
