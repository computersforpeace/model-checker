#ifndef CYCLEGRAPH_H
#define CYCLEGRAPH_H

#include "hashtable.h"
#include "action.h"
#include <vector>

class CycleNode;

/** @brief A graph of Model Actions for tracking cycles. */
class CycleGraph {
 public:
	CycleGraph();
	void addEdge(ModelAction *from, ModelAction *to);
	bool checkForCycles();

 private:
	CycleNode * getNode(ModelAction *);
	HashTable<class ModelAction *, class CycleNode *, uintptr_t, 4> actionToNode;
	bool checkReachable(CycleNode *from, CycleNode *to);
	
	bool hasCycles;

};

class CycleNode {
 public:
	CycleNode(ModelAction *action);
	void addEdge(CycleNode * node);
	std::vector<class CycleNode *> * getEdges();

 private:
	ModelAction *action;
 	std::vector<class CycleNode *> edges;
};

#endif
