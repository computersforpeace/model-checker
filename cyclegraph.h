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
	void addEdge(ModelAction *from, ModelAction *to);
	bool checkForCycles();

 private:
	CycleNode * getNode(ModelAction *);
	HashTable<ModelAction *, CycleNode *, uintptr_t, 4> actionToNode;
	bool checkReachable(CycleNode *from, CycleNode *to);
	bool hasCycles;

};

class CycleNode {
 public:
	CycleNode(ModelAction *action);
	void addEdge(CycleNode * node);
	std::vector<CycleNode *> * getEdges();

 private:
	ModelAction *action;
	std::vector<CycleNode *> edges;
};

#endif
