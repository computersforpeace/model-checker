/** @file snapshot-interface.h
 *  @brief C++ layer on top of snapshotting system.
 */

#ifndef __SNAPINTERFACE_H
#define __SNAPINTERFACE_H

typedef unsigned int snapshot_id;

typedef void (*VoidFuncPtr)();
void initSnapshotLibrary(unsigned int numbackingpages,
		unsigned int numsnapshots, unsigned int nummemoryregions,
		unsigned int numheappages, VoidFuncPtr entryPoint);

void snapshot_stack_init();
void snapshot_record(int seq_index);
int snapshot_backtrack_before(int seq_index);

#endif
