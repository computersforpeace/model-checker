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
typedef std::basic_string< char, std::char_traits< char >, MyAlloc< char > > MyString;
namespace snapshot_utils{
/*
SnapshotTree defines the interfaces and ideal points that require snapshotting.
The algorithm is: When a step happens we create a new Tree node and define that node as the current scope.
when it is time to set a backtracking point, for each backtracking point we record its parent
finally when set current scope is called, we see if this state is a parent 
where possible the goal is speed so we use an integer comparison to make all comparison operations required for 
finding a given node O 1
*/
	typedef std::map< thrd_t, int, std::less< thrd_t >, MyAlloc< std::pair< const thrd_t, int > > > ThreadStepMap_t;
	MyString SerializeThreadSteps( const ThreadStepMap_t & theMap );
	class snapshotTree;
	struct snapshotTreeComp{
		public:
			bool operator()( const std::pair< const snapshotTree*, snapshot_id > & lhs, const std::pair< const snapshotTree*, snapshot_id > & rhs );	
	};
	
	typedef std::map< snapshotTree *, snapshot_id, snapshotTreeComp, MyAlloc< std::pair< const snapshotTree *, snapshot_id > > > SnapshotToStateMap_t;
	typedef std::set< snapshotTree*, std::less< snapshotTree * >, MyAlloc< snapshotTree > > BackTrackingParents_t; 
	typedef std::vector< snapshotTree *, MyAlloc< snapshotTree * > > SnapshotChildren_t;
	typedef std::map< MyString, snapshotTree *, std::less<  MyString >, MyAlloc< std::pair< const MyString, snapshotTree * > > > SnapshotEdgeMap_t;
    void SnapshotGlobalSegments();
    class snapshotTree{
		friend struct snapshotTreeComp;
	public:
		MEMALLOC
		explicit snapshotTree( );
		~snapshotTree();
		static snapshotTree * ReturnCurrentScope();
		std::pair< MyString, bool > TakeStep( thrd_t which, thrd_t whoseNext );
	private: 
		unsigned int mTimeStamp;
		ThreadStepMap_t mThreadStepsTaken;
		thrd_t mNextStepTaker;
		snapshotTree * mpParent;
		SnapshotChildren_t mChildren;
		void AddThreadStep( ThreadStepMap_t & theMap, thrd_t theThread );
		static snapshotTree * msCurrentScope;
		static BackTrackingParents_t msRecordedParents;
		static SnapshotToStateMap_t msSnapshottedStates;
		static SnapshotEdgeMap_t msSnapshotEdgesMap; //this might not actually needed, if this is blowing up memory we have to traverse and find as opposed to looking up....
		static unsigned int msTimeCounter;
        void EnsureRelevantRegionsSnapshotted();
	public:
		void BacktrackingPointSet();
		bool operator<( const snapshotTree & rhs ) const;
		snapshot_id ReturnEarliestSnapshotID( MyString key );
		snapshot_id SnapshotNow();
	};
};
#endif
