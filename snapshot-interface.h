#ifndef __SNAPINTERFACE_H
#define __SNAPINTERFACE_H
#include "mymemory.h"

typedef unsigned int snapshot_id;

typedef void (*VoidFuncPtr)();
void initSnapShotLibrary(unsigned int numbackingpages,
		unsigned int numsnapshots, unsigned int nummemoryregions,
		unsigned int numheappages, VoidFuncPtr entryPoint);
void finalize();

void SnapshotGlobalSegments();

struct stackEntry {
  struct stackEntry *next;
  snapshot_id snapshotid;
  int index;
};

class SnapshotStack {
 public:
  MEMALLOC
  SnapshotStack( );
  ~SnapshotStack();
  int backTrackBeforeStep(int seq_index);
  void snapshotStep(int seq_index);

 private: 
  struct stackEntry * stack;
};

/* Not sure what it even means to have more than one snapshot object,
   so let's just make a global reference to it.*/

extern SnapshotStack * snapshotObject;
#endif
