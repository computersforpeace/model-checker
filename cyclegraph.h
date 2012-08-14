/** @file cyclegraph.h @brief Data structure to track ordering
 *  constraints on modification order.  The idea is to see whether a
 *  total order exists that satisfies the ordering constriants.*/

#ifndef CYCLEGRAPH_H
#define CYCLEGRAPH_H

#include "hashtable.h"
#include <vector>
#include <inttypes.h>

class CycleNode;
class ModelAction;

/** @brief A graph of Model Actions for tracking cycles. */
class CycleGraph {
 public:
	CycleGraph();
	~CycleGraph();
	void addEdge(const ModelAction *from, const ModelAction *to);
	bool checkForCycles();
	void addRMWEdge(const ModelAction *from, const ModelAction *to);

 private:
	CycleNode * getNode(const ModelAction *);

	/** @brief A table for mapping ModelActions to CycleNodes */
	HashTable<const ModelAction *, CycleNode *, uintptr_t, 4> actionToNode;

	bool checkReachable(CycleNode *from, CycleNode *to);

	/** @brief A flag: true if this graph contains cycles */
	bool hasCycles;
};

/** @brief A node within a CycleGraph; corresponds to one ModelAction */
class CycleNode {
 public:
	CycleNode(const ModelAction *action);
	void addEdge(CycleNode * node);
	std::vector<CycleNode *> * getEdges();
	bool setRMW(CycleNode *);
	CycleNode* getRMW();

 private:
	/** @brief The ModelAction that this node represents */
	const ModelAction *action;

	/** @brief The edges leading out from this node */
	std::vector<CycleNode *> edges;

	/** Pointer to a RMW node that reads from this node, or NULL, if none
	 * exists */
	CycleNode * hasRMW;
};

#endif
