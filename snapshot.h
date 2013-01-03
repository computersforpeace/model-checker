/** @file snapshot.h
 *	@brief Snapshotting interface header file.
 */

#ifndef _SNAPSHOT_H
#define _SNAPSHOT_H

#include "snapshot-interface.h"
#include "config.h"
#include "mymemory.h"

void snapshot_add_memory_region(void *ptr, unsigned int numPages);
snapshot_id take_snapshot();
void snapshot_roll_back(snapshot_id theSnapShot);

#if !USE_MPROTECT_SNAPSHOT
mspace create_shared_mspace();
#endif

#endif
