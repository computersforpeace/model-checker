#ifndef __SNAPINTERFACE_H
#define __SNAPINTERFACE_H
#include "snapshot.h"
#include "mymemory.h"
#include <vector>
#include <utility>
#include <string>
#include <map>
#include <set>
#include "snapshot.h"
#include "libthreads.h"

class snapshotStack;
typedef std::basic_string<char, std::char_traits<char>, MyAlloc<char> > MyString;

void SnapshotGlobalSegments();

struct stackEntry {
  struct stackEntry *next;
  snapshot_id snapshotid;
  int index;
};

class snapshotStack {
 public:
  MEMALLOC
  snapshotStack( );
  ~snapshotStack();
  int backTrackBeforeStep(int seq_index);
  void snapshotStep(int seq_index);

 private: 
  struct stackEntry * stack;
};
#endif
