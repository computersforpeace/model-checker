#include "cyclegraph.h"
#include "action.h"

CycleGraph::CycleGraph() {
	hasCycles=false;
}

CycleNode * CycleGraph::getNode(ModelAction * action) {
	CycleNode *node=actionToNode.get(action);
	if (node==NULL) {
		node=new CycleNode(action);
		actionToNode.put(action, node);
	}
	return node;
}

void CycleGraph::addEdge(ModelAction *from, ModelAction *to) {
	CycleNode *fromnode=getNode(from);
	CycleNode *tonode=getNode(to);
	if (!hasCycles) {
		// Check for Cycles
		hasCycles=checkReachable(fromnode, tonode);
	}
	fromnode->addEdge(tonode);
}

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

CycleNode::CycleNode(ModelAction *modelaction) {
	action=modelaction;
}

std::vector<CycleNode *> * CycleNode::getEdges() {
	return &edges;
}

void CycleNode::addEdge(CycleNode * node) {
	edges.push_back(node);
}
