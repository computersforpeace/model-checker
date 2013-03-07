#ifndef __STL_MODEL_H__
#define __STL_MODEL_H__

#include <vector>
#include <list>
#include "mymemory.h"

template<typename _Tp>
class ModelList : public std::list<_Tp, ModelAlloc<_Tp> >
{
 public:
	typedef std::list< _Tp, ModelAlloc<_Tp> > list;

	ModelList() :
		list()
	{ }

	ModelList(size_t n, const _Tp& val = _Tp()) :
		list(n, val)
	{ }

	MEMALLOC
};

template<typename _Tp>
class SnapList : public std::list<_Tp, SnapshotAlloc<_Tp> >
{
 public:
	typedef std::list<_Tp, SnapshotAlloc<_Tp> > list;

	SnapList() :
		list()
	{ }

	SnapList(size_t n, const _Tp& val = _Tp()) :
		list(n, val)
	{ }

	SNAPSHOTALLOC
};

template<typename _Tp>
class ModelVector : public std::vector<_Tp, ModelAlloc<_Tp> >
{
 public:
	typedef std::vector< _Tp, ModelAlloc<_Tp> > vector;

	ModelVector() :
		vector()
	{ }

	ModelVector(size_t n, const _Tp& val = _Tp()) :
		vector(n, val)
	{ }

	MEMALLOC
};

template<typename _Tp>
class SnapVector : public std::vector<_Tp, SnapshotAlloc<_Tp> >
{
 public:
	typedef std::vector< _Tp, SnapshotAlloc<_Tp> > vector;

	SnapVector() :
		vector()
	{ }

	SnapVector(size_t n, const _Tp& val = _Tp()) :
		vector(n, val)
	{ }

	SNAPSHOTALLOC
};

#endif /* __STL_MODEL_H__ */
