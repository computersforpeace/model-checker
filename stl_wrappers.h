#ifndef STL_WRAPPERS_H
#define STL_WRAPPERS_H

#include <vector>
#include <list>
#include "mymemory.h"

template<typename _Tp >
class snap_vector:public std::vector<_Tp, SnapshotAlloc<_Tp> > {
 public:
 snap_vector() : std::vector<_Tp, SnapshotAlloc<_Tp> >() {
	}
 snap_vector(int __n) : std::vector<_Tp, SnapshotAlloc<_Tp> >(__n) {
	}

	SNAPSHOTALLOC
};

template<typename _Tp >
class model_vector:public std::vector<_Tp, ModelAlloc<_Tp> > {
 public:
	MEMALLOC
};

template<typename _Tp >
class snap_list:public std::list<_Tp, SnapshotAlloc<_Tp> > {
 public:
	SNAPSHOTALLOC
};

template<typename _Tp >
class model_list:public std::list<_Tp, ModelAlloc<_Tp> > {
 public:
 model_list() : std::list<_Tp, ModelAlloc<_Tp> >() {
	}
 model_list(int __n, _Tp t) : std::list<_Tp, ModelAlloc<_Tp> >(__n, t) {
	}
	MEMALLOC
};
#endif
