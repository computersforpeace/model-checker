#include "cyclegraph.h"
#include "action.h"
#include "common.h"

/** Initializes a CycleGraph object. */
CycleGraph::CycleGraph() :
	hasCycles(false),
	oldCycles(false),
	hasRMWViolation(false),
	oldRMWViolation(false)
{
}

/** CycleGraph destructor */
CycleGraph::~CycleGraph() {
}

/**
 * @brief Returns the CycleNode corresponding to a given ModelAction
 * @param action The ModelAction to find a node for
 * @return The CycleNode paired with this action
 */
CycleNode * CycleGraph::getNode(const ModelAction *action) {
	CycleNode *node=actionToNode.get(action);
	if (node==NULL) {
		node=new CycleNode(action);
		actionToNode.put(action, node);
	}
	return node;
}

/**
 * Adds an edge between two ModelActions. The ModelAction @a to is ordered
 * after the ModelAction @a from.
 * @param to The edge points to this ModelAction
 * @param from The edge comes from this ModelAction
 */
void CycleGraph::addEdge(const ModelAction *from, const ModelAction *to) {
	ASSERT(from);
	ASSERT(to);

	CycleNode *fromnode=getNode(from);
	CycleNode *tonode=getNode(to);

	if (!hasCycles) {
		// Check for Cycles
		hasCycles=checkReachable(tonode, fromnode);
	}

	rollbackvector.push_back(fromnode);
	fromnode->addEdge(tonode);

	CycleNode * rmwnode=fromnode->getRMW();

	//If the fromnode has a rmwnode that is not the tonode, we
	//should add an edge between its rmwnode and the tonode

	if (rmwnode!=NULL&&rmwnode!=tonode) {
		if (!hasCycles) {
			// Check for Cycles
			hasCycles=checkReachable(tonode, rmwnode);
		}
		rollbackvector.push_back(rmwnode);
		rmwnode->addEdge(tonode);
	}
}

/** Handles special case of a RMW action.  The ModelAction rmw reads
 *  from the ModelAction from.  The key differences are: (1) no write
 *  can occur in between the rmw and the from action.  Only one RMW
 *  action can read from a given write.
 */
void CycleGraph::addRMWEdge(const ModelAction *from, const ModelAction *rmw) {
	ASSERT(from);
	ASSERT(rmw);

	CycleNode *fromnode=getNode(from);
	CycleNode *rmwnode=getNode(rmw);

	/* Two RMW actions cannot read from the same write. */
	if (fromnode->setRMW(rmwnode)) {
		hasRMWViolation=true;
	} else {
		rmwrollbackvector.push_back(fromnode);
	}

	/* Transfer all outgoing edges from the from node to the rmw node */
	/* This process should not add a cycle because either: 
	 * (1) The rmw should not have any incoming edges yet if it is the
	 * new node or
	 * (2) the fromnode is the new node and therefore it should not 
	 * have any outgoing edges.
	 */
	std::vector<CycleNode *> * edges=fromnode->getEdges();
	for(unsigned int i=0;i<edges->size();i++) {
		CycleNode * tonode=(*edges)[i];
		rollbackvector.push_back(rmwnode);
		rmwnode->addEdge(tonode);
	}
	rollbackvector.push_back(fromnode);

	if (!hasCycles) {
		// With promises we could be setting up a cycle here if we aren't
		// careful...avoid it..
		hasCycles=checkReachable(rmwnode, fromnode);
	}
	fromnode->addEdge(rmwnode);
}

/**
 * Checks whether one ModelAction can reach another.
 * @param from The ModelAction from which to begin exploration
 * @param to The ModelAction to reach
 * @return True, @a from can reach @a to; otherwise, false
 */
bool CycleGraph::checkReachable(const ModelAction *from, const ModelAction *to) {
	CycleNode *fromnode = actionToNode.get(from);
	CycleNode *tonode = actionToNode.get(to);

	if (!fromnode || !tonode)
		return false;

	return checkReachable(fromnode, tonode);
}

/**
 * Checks whether one CycleNode can reach another.
 * @param from The CycleNode from which to begin exploration
 * @param to The CycleNode to reach
 * @return True, @a from can reach @a to; otherwise, false
 */
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

void CycleGraph::startChanges() {
	ASSERT(rollbackvector.size()==0);
	ASSERT(rmwrollbackvector.size()==0);
	ASSERT(oldCycles==hasCycles);
	ASSERT(oldRMWViolation==hasRMWViolation);
}

/** Commit changes to the cyclegraph. */
void CycleGraph::commitChanges() {
	rollbackvector.resize(0);
	rmwrollbackvector.resize(0);
	oldCycles=hasCycles;
	oldRMWViolation=hasRMWViolation;
}

/** Rollback changes to the previous commit. */
void CycleGraph::rollbackChanges() {
	for (unsigned int i = 0; i < rollbackvector.size(); i++) {
		rollbackvector[i]->popEdge();
	}

	for (unsigned int i = 0; i < rmwrollbackvector.size(); i++) {
		rmwrollbackvector[i]->clearRMW();
	}

	hasCycles = oldCycles;
	hasRMWViolation = oldRMWViolation;
	rollbackvector.resize(0);
	rmwrollbackvector.resize(0);
}

/** @returns whether a CycleGraph contains cycles. */
bool CycleGraph::checkForCycles() {
	return hasCycles;
}

bool CycleGraph::checkForRMWViolation() {
	return hasRMWViolation;
}

/**
 * Constructor for a CycleNode.
 * @param modelaction The ModelAction for this node
 */
CycleNode::CycleNode(const ModelAction *modelaction) :
	action(modelaction),
	hasRMW(NULL)
{
}

/** @returns a vector of the edges from a CycleNode. */
std::vector<CycleNode *> * CycleNode::getEdges() {
	return &edges;
}

/**
 * Adds an edge from this CycleNode to another CycleNode.
 * @param node The node to which we add a directed edge
 */
void CycleNode::addEdge(CycleNode *node) {
	edges.push_back(node);
}

/** @returns the RMW CycleNode that reads from the current CycleNode */
CycleNode * CycleNode::getRMW() {
	return hasRMW;
}

/**
 * Set a RMW action node that reads from the current CycleNode.
 * @param node The RMW that reads from the current node
 * @return True, if this node already was read by another RMW; false otherwise
 * @see CycleGraph::addRMWEdge
 */
bool CycleNode::setRMW(CycleNode *node) {
	if (hasRMW!=NULL)
		return true;
	hasRMW=node;
	return false;
}
