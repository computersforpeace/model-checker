/** @file snapshot.h
 *	@brief Snapshotting interface header file.
 */

#ifndef _SNAPSHOT_H
#define _SNAPSHOT_H

#include "snapshot-interface.h"

#define PAGESIZE 4096

/* If USE_MPROTECT_SNAPSHOT=1, then snapshot by using mmap() and mprotect()
   If USE_MPROTECT_SNAPSHOT=0, then snapshot by using fork() */
#define USE_MPROTECT_SNAPSHOT 1

/* Size of signal stack */
#define SIGSTACKSIZE 32768

void addMemoryRegionToSnapShot( void * ptr, unsigned int numPages );

snapshot_id takeSnapshot( );

void rollBack( snapshot_id theSnapShot );

void createSharedLibrary();

#endif
