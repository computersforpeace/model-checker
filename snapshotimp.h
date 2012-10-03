/** @file snapshotimp.h
 *	@brief Snapshotting implementation header file..
 */

#ifndef _SNAPSHOTIMP_H
#define _SNAPSHOTIMP_H
#include "snapshot.h"
#include <iostream>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <csignal>
#define SHARED_MEMORY_DEFAULT  (100 * ((size_t)1 << 20)) // 100mb for the shared memory
#define STACK_SIZE_DEFAULT      (((size_t)1 << 20) * 20)  // 20 mb out of the above 100 mb for my stack

#if USE_MPROTECT_SNAPSHOT
//Each snapshotrecord lists the firstbackingpage that must be written to revert to that snapshot
struct SnapShotRecord {
	unsigned int firstBackingPage;
};

//Backing store page struct
struct SnapShotPage {
	char data[PAGESIZE];
};

//List the base address of the corresponding page in the backing store so we know where to copy it to
struct BackingPageRecord {
	void * basePtrOfPage;
};

//Stuct for each memory region
struct MemoryRegion {
	void * basePtr; //base of memory region
	int sizeInPages; //size of memory region in pages
};

//Primary struct for snapshotting system....
struct SnapShot {
	struct MemoryRegion * regionsToSnapShot; //This pointer references an array of memory regions to snapshot
	struct SnapShotPage * backingStore; //This pointer references an array of snapshotpage's that form the backing store
	void * backingStoreBasePtr; //This pointer references an array of snapshotpage's that form the backing store
	struct BackingPageRecord * backingRecords; //This pointer references an array of backingpagerecord's (same number of elements as backingstore
	struct SnapShotRecord * snapShots; //This pointer references the snapshot array

	unsigned int lastSnapShot; //Stores the next snapshot record we should use
	unsigned int lastBackingPage; //Stores the next backingpage we should use
	unsigned int lastRegion; //Stores the next memory region to be used

	unsigned int maxRegions; //Stores the max number of memory regions we support
	unsigned int maxBackingPages; //Stores the total number of backing pages
	unsigned int maxSnapShots; //Stores the total number of snapshots we allow
};

#else
struct SnapShot {
	void *mSharedMemoryBase;
	void *mStackBase;
	size_t mStackSize;
	volatile snapshot_id mIDToRollback;
	ucontext_t mContextToRollback;
	snapshot_id currSnapShotID;
};
#endif

//Global reference to snapshot data structure
extern struct SnapShot * snapshotrecord;
#endif
