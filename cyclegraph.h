/**
 * @file cyclegraph.h
 * @brief Data structure to track ordering constraints on modification order
 *
 * Used to determine whether a total order exists that satisfies the ordering
 * constraints.
 */

#ifndef __CYCLEGRAPH_H__
#define __CYCLEGRAPH_H__

#include "hashtable.h"
#include <vector>
#include <inttypes.h>
#include "config.h"
#include "mymemory.h"

class Promise;
class CycleNode;
class ModelAction;

typedef std::vector< const Promise *, ModelAlloc<const Promise *> > promise_list_t;

/** @brief A graph of Model Actions for tracking cycles. */
class CycleGraph {
 public:
	CycleGraph();
	~CycleGraph();

	template <typename T, typename U>
	bool addEdge(const T from, const U to);

	template <typename T>
	void addRMWEdge(const T *from, const ModelAction *rmw);

	bool checkForCycles() const;
	bool checkPromise(const ModelAction *from, Promise *p) const;

	template <typename T>
	bool checkReachable(const ModelAction *from, const T *to) const;

	void startChanges();
	void commitChanges();
	void rollbackChanges();
#if SUPPORT_MOD_ORDER_DUMP
	void dumpNodes(FILE *file) const;
	void dumpGraphToFile(const char *filename) const;
#endif

	bool resolvePromise(ModelAction *reader, ModelAction *writer,
			promise_list_t *mustResolve);

	SNAPSHOTALLOC
 private:
	bool addNodeEdge(CycleNode *fromnode, CycleNode *tonode);
	void putNode(const ModelAction *act, CycleNode *node);
	CycleNode * getNode(const ModelAction *act);
	CycleNode * getNode(const Promise *promise);
	CycleNode * getNode_noCreate(const ModelAction *act) const;
	CycleNode * getNode_noCreate(const Promise *promise) const;
	bool mergeNodes(CycleNode *node1, CycleNode *node2,
			promise_list_t *mustMerge);

	HashTable<const CycleNode *, const CycleNode *, uintptr_t, 4, model_malloc, model_calloc, model_free> *discovered;

	/** @brief A table for mapping ModelActions to CycleNodes */
	HashTable<const ModelAction *, CycleNode *, uintptr_t, 4> actionToNode;
	/** @brief A table for mapping reader ModelActions to Promise
	 *  CycleNodes */
	HashTable<const ModelAction *, CycleNode *, uintptr_t, 4> readerToPromiseNode;

#if SUPPORT_MOD_ORDER_DUMP
	std::vector<CycleNode *> nodeList;
#endif

	bool checkReachable(const CycleNode *from, const CycleNode *to) const;

	/** @brief A flag: true if this graph contains cycles */
	bool hasCycles;
	bool oldCycles;

	std::vector< CycleNode *, SnapshotAlloc<CycleNode *> > rollbackvector;
	std::vector< CycleNode *, SnapshotAlloc<CycleNode *> > rmwrollbackvector;
};

/**
 * @brief A node within a CycleGraph; corresponds either to one ModelAction or
 * to a promised future value
 */
class CycleNode {
 public:
	CycleNode(const ModelAction *act);
	CycleNode(const Promise *promise);
	bool addEdge(CycleNode *node);
	CycleNode * getEdge(unsigned int i) const;
	unsigned int getNumEdges() const;
	CycleNode * getBackEdge(unsigned int i) const;
	unsigned int getNumBackEdges() const;
	CycleNode * removeEdge();
	CycleNode * removeBackEdge();

	bool setRMW(CycleNode *);
	CycleNode * getRMW() const;
	void clearRMW() { hasRMW = NULL; }
	const ModelAction * getAction() const { return action; }
	const Promise * getPromise() const { return promise; }
	bool is_promise() const { return !action; }
	void resolvePromise(const ModelAction *writer);

	SNAPSHOTALLOC
 private:
	/** @brief The ModelAction that this node represents */
	const ModelAction *action;

	/** @brief The promise represented by this node; only valid when action
	 * is NULL */
	const Promise *promise;

	/** @brief The edges leading out from this node */
	std::vector< CycleNode *, SnapshotAlloc<CycleNode *> > edges;

	/** @brief The edges leading into this node */
	std::vector< CycleNode *, SnapshotAlloc<CycleNode *> > back_edges;

	/** Pointer to a RMW node that reads from this node, or NULL, if none
	 * exists */
	CycleNode *hasRMW;
};

/*
 * @brief Adds an edge between objects
 *
 * This function will add an edge between any two objects which can be
 * associated with a CycleNode. That is, if they have a CycleGraph::getNode
 * implementation.
 *
 * The object to is ordered after the object from.
 *
 * @param to The edge points to this object, of type T
 * @param from The edge comes from this object, of type U
 * @return True, if new edge(s) are added; otherwise false
 */
template <typename T, typename U>
bool CycleGraph::addEdge(const T from, const U to)
{
	ASSERT(from);
	ASSERT(to);

	CycleNode *fromnode = getNode(from);
	CycleNode *tonode = getNode(to);

	return addNodeEdge(fromnode, tonode);
}

/**
 * Checks whether one ModelAction can reach another ModelAction/Promise
 * @param from The ModelAction from which to begin exploration
 * @param to The ModelAction or Promise to reach
 * @return True, @a from can reach @a to; otherwise, false
 */
template <typename T>
bool CycleGraph::checkReachable(const ModelAction *from, const T *to) const
{
	CycleNode *fromnode = getNode_noCreate(from);
	CycleNode *tonode = getNode_noCreate(to);

	if (!fromnode || !tonode)
		return false;

	return checkReachable(fromnode, tonode);
}

#endif /* __CYCLEGRAPH_H__ */
