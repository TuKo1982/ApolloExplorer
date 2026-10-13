/*
 * DirectoryList.h
 *
 *  Created on: May 21, 2021
 *      Author: rony
 */

#ifndef AMIGA_DIRECTORYLIST_H_
#define AMIGA_DIRECTORYLIST_H_

#include "protocolTypes.h"

//Returns a newly allocated message (or NULL).  The caller must FreeVec() it.
ProtocolMessageDirectoryList_t *getDirectoryList( char *path );

#endif /* AMIGA_DIRECTORYLIST_H_ */
