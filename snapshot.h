/** @file snapshot.h
 *	@brief Snapshotting interface header file.
 */

#ifndef _SNAPSHOT_H
#define _SNAPSHOT_H

#include "snapshot-interface.h"
#include "config.h"

void addMemoryRegionToSnapShot(void *ptr, unsigned int numPages);
snapshot_id takeSnapshot();
void rollBack(snapshot_id theSnapShot);

#if !USE_MPROTECT_SNAPSHOT
void createSharedMemory();
#endif

#endif
