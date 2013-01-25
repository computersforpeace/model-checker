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

/** @brief A graph of Model Actions for tracking cycles. */
class CycleGraph {
 public:
	CycleGraph();
	~CycleGraph();
	void addEdge(const ModelAction *from, const ModelAction *to);
	bool checkForCycles() const;
	bool checkForRMWViolation() const;
	void addRMWEdge(const ModelAction *from, const ModelAction *rmw);
	bool checkPromise(const ModelAction *from, Promise *p) const;
	bool checkReachable(const ModelAction *from, const ModelAction *to) const;
	void startChanges();
	void commitChanges();
	void rollbackChanges();
#if SUPPORT_MOD_ORDER_DUMP
	void dumpNodes(FILE *file) const;
	void dumpGraphToFile(const char *filename) const;
#endif

	SNAPSHOTALLOC
 private:
	void addEdge(CycleNode *fromnode, CycleNode *tonode);
	void putNode(const ModelAction *act, CycleNode *node);
	CycleNode * getNode(const ModelAction *);
	HashTable<const CycleNode *, const CycleNode *, uintptr_t, 4, model_malloc, model_calloc, model_free> *discovered;

	/** @brief A table for mapping ModelActions to CycleNodes */
	HashTable<const ModelAction *, CycleNode *, uintptr_t, 4> actionToNode;
#if SUPPORT_MOD_ORDER_DUMP
	std::vector<CycleNode *> nodeList;
#endif

	bool checkReachable(const CycleNode *from, const CycleNode *to) const;

	/** @brief A flag: true if this graph contains cycles */
	bool hasCycles;
	bool oldCycles;

	bool hasRMWViolation;
	bool oldRMWViolation;

	std::vector< CycleNode *, SnapshotAlloc<CycleNode *> > rollbackvector;
	std::vector< CycleNode *, SnapshotAlloc<CycleNode *> > rmwrollbackvector;
};

/** @brief A node within a CycleGraph; corresponds to one ModelAction */
class CycleNode {
 public:
	CycleNode(const ModelAction *act);
	bool addEdge(CycleNode *node);
	CycleNode * getEdge(unsigned int i) const;
	unsigned int getNumEdges() const;
	CycleNode * getBackEdge(unsigned int i) const;
	unsigned int getNumBackEdges() const;
	bool setRMW(CycleNode *);
	CycleNode * getRMW() const;
	const ModelAction * getAction() const { return action; }

	void popEdge() {
		edges.pop_back();
	}
	void clearRMW() {
		hasRMW = NULL;
	}

	SNAPSHOTALLOC
 private:
	/** @brief The ModelAction that this node represents */
	const ModelAction *action;

	/** @brief The edges leading out from this node */
	std::vector< CycleNode *, SnapshotAlloc<CycleNode *> > edges;

	/** @brief The edges leading into this node */
	std::vector< CycleNode *, SnapshotAlloc<CycleNode *> > back_edges;

	/** Pointer to a RMW node that reads from this node, or NULL, if none
	 * exists */
	CycleNode *hasRMW;
};

#endif /* __CYCLEGRAPH_H__ */
