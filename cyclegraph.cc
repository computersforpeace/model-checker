#include "cyclegraph.h"
#include "action.h"

/** Initializes a CycleGraph object. */
CycleGraph::CycleGraph() {
	hasCycles=false;
}

CycleGraph::~CycleGraph() {
}

/** Returns the CycleNode for a given ModelAction. */
CycleNode * CycleGraph::getNode(const ModelAction * action) {
	CycleNode *node=actionToNode.get(action);
	if (node==NULL) {
		node=new CycleNode(action);
		actionToNode.put(action, node);
	}
	return node;
}

/** Adds an edge between two ModelActions. */
void CycleGraph::addEdge(const ModelAction *from, const ModelAction *to) {
	CycleNode *fromnode=getNode(from);
	CycleNode *tonode=getNode(to);
	if (!hasCycles) {
		// Check for Cycles
		hasCycles=checkReachable(tonode, fromnode);
	}
	fromnode->addEdge(tonode);
}

void CycleGraph::addRMWEdge(const ModelAction *from, const ModelAction *rmw) {
	CycleNode *fromnode=getNode(from);
	CycleNode *rmwnode=getNode(rmw);

	/* Two RMW actions cannot read from the same write. */
	if (fromnode->setRMW())
		hasCycles=true;

	/* Transfer all outgoing edges from the from node to the rmw node */
	/* This process cannot add a cycle because rmw should not have any
		 incoming edges yet.*/
	std::vector<CycleNode *> * edges=fromnode->getEdges();
	for(unsigned int i=0;edges->size();i++) {
		CycleNode * tonode=(*edges)[i];
		rmwnode->addEdge(tonode);
	}
	if (!hasCycles) {
		hasCycles=checkReachable(rmwnode, fromnode);
	}
	fromnode->addEdge(rmwnode);
}


/** Checks whether the first CycleNode can reach the second one. */
bool CycleGraph::checkReachable(CycleNode *from, CycleNode *to) {
	std::vector<CycleNode *> queue;
	HashTable<CycleNode *, CycleNode *, uintptr_t, 4> discovered;

	queue.push_back(from);
	discovered.put(from, from);
	while(!queue.empty()) {
		CycleNode * node=queue.back();
		queue.pop_back();
		if (node==to)
			return true;

		for(unsigned int i=0;i<node->getEdges()->size();i++) {
			CycleNode *next=(*node->getEdges())[i];
			if (!discovered.contains(next)) {
				discovered.put(next,next);
				queue.push_back(next);
			}
		}
	}
	return false;
}

/** Returns whether a CycleGraph contains cycles. */
bool CycleGraph::checkForCycles() {
	return hasCycles;
}

/** Constructor for a CycleNode. */
CycleNode::CycleNode(const ModelAction *modelaction) {
	action=modelaction;
}

/** Returns a vector of the edges from a CycleNode. */
std::vector<CycleNode *> * CycleNode::getEdges() {
	return &edges;
}

/** Adds an edge to a CycleNode. */
void CycleNode::addEdge(CycleNode * node) {
	edges.push_back(node);
}

bool CycleNode::setRMW() {
	bool oldhasRMW=hasRMW;
	hasRMW=true;
	return oldhasRMW;
}
