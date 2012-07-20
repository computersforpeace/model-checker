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
	HashTable<const ModelAction *, CycleNode *, uintptr_t, 4> actionToNode;
	bool checkReachable(CycleNode *from, CycleNode *to);
	bool hasCycles;
};

class CycleNode {
 public:
	CycleNode(const ModelAction *action);
	void addEdge(CycleNode * node);
	std::vector<CycleNode *> * getEdges();
	bool setRMW(CycleNode *);
	CycleNode* getRMW();

 private:
	const ModelAction *action;
	std::vector<CycleNode *> edges;
	CycleNode * hasRMW;
};

#endif
