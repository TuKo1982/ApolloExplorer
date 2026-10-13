/*
 * VolumeList.h
 *
 *  Created on: May 28, 2021
 *      Author: rony
 */

#ifndef AMIGA_VOLUMELIST_H_
#define AMIGA_VOLUMELIST_H_

#include "protocolTypes.h"

//Returns a newly allocated message (or NULL).  The caller must FreeVec() it.
ProtocolMessage_VolumeList_t *getVolumeList();

#endif /* AMIGA_VOLUMELIST_H_ */
