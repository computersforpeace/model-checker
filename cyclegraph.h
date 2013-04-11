/**
 * @file cyclegraph.h
 * @brief Data structure to track ordering constraints on modification order
 *
 * Used to determine whether a total order exists that satisfies the ordering
 * constraints.
 */

#ifndef __CYCLEGRAPH_H__
#define __CYCLEGRAPH_H__

#include <inttypes.h>
#include <stdio.h>

#include "hashtable.h"
#include "config.h"
#include "mymemory.h"
#include "stl-model.h"

class Promise;
class CycleNode;
class ModelAction;

/** @brief A graph of Model Actions for tracking cycles. */
class CycleGraph {
 public:
	CycleGraph();
	~CycleGraph();

	template <typename T, typename U>
	bool addEdge(const T *from, const U *to);

	template <typename T>
	void addRMWEdge(const T *from, const ModelAction *rmw);

	bool checkForCycles() const;
	bool checkPromise(const ModelAction *from, Promise *p) const;

	template <typename T, typename U>
	bool checkReachable(const T *from, const U *to) const;

	void startChanges();
	void commitChanges();
	void rollbackChanges();
#if SUPPORT_MOD_ORDER_DUMP
	void dumpNodes(FILE *file) const;
	void dumpGraphToFile(const char *filename) const;

	void dot_print_node(FILE *file, const ModelAction *act);
	template <typename T, typename U>
	void dot_print_edge(FILE *file, const T *from, const U *to, const char *prop);
#endif

	bool resolvePromise(const Promise *promise, ModelAction *writer);

	SNAPSHOTALLOC
 private:
	bool addNodeEdge(CycleNode *fromnode, CycleNode *tonode);
	void putNode(const ModelAction *act, CycleNode *node);
	void putNode(const Promise *promise, CycleNode *node);
	void erasePromiseNode(const Promise *promise);
	CycleNode * getNode(const ModelAction *act);
	CycleNode * getNode(const Promise *promise);
	CycleNode * getNode_noCreate(const ModelAction *act) const;
	CycleNode * getNode_noCreate(const Promise *promise) const;
	bool mergeNodes(CycleNode *node1, CycleNode *node2);

	HashTable<const CycleNode *, const CycleNode *, uintptr_t, 4, model_malloc, model_calloc, model_free> *discovered;
	ModelVector<const CycleNode *> * queue;


	/** @brief A table for mapping ModelActions to CycleNodes */
	HashTable<const ModelAction *, CycleNode *, uintptr_t, 4> actionToNode;
	/** @brief A table for mapping Promises to CycleNodes */
	HashTable<const Promise *, CycleNode *, uintptr_t, 4> promiseToNode;

#if SUPPORT_MOD_ORDER_DUMP
	SnapVector<CycleNode *> nodeList;
#endif

	bool checkReachable(const CycleNode *from, const CycleNode *to) const;

	/** @brief A flag: true if this graph contains cycles */
	bool hasCycles;
	/** @brief The previous value of CycleGraph::hasCycles, for rollback */
	bool oldCycles;

	SnapVector<CycleNode *> rollbackvector;
	SnapVector<CycleNode *> rmwrollbackvector;
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
	SnapVector<CycleNode *> edges;

	/** @brief The edges leading into this node */
	SnapVector<CycleNode *> back_edges;

	/** Pointer to a RMW node that reads from this node, or NULL, if none
	 * exists */
	CycleNode *hasRMW;
};

#endif /* __CYCLEGRAPH_H__ */
