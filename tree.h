#ifndef __TREE_H__
#define __TREE_H__

#include <set>
#include <map>
#include "threads.h"

/*
 * An n-ary tree
 *
 * A tree with n possible branches from each node - used for recording the
 * execution paths we've executed / backtracked
 */
class TreeNode {
public:
	TreeNode(TreeNode *par);
	~TreeNode();
	bool hasBeenExplored(thread_id_t id) { return children.find(id_to_int(id)) != children.end(); }
	TreeNode * exploreChild(thread_id_t id);
	thread_id_t getNextBacktrack();

	/* Return 1 if already in backtrack, 0 otherwise */
	int setBacktrack(thread_id_t id);
	TreeNode * getRoot();
	static int getTotalNodes() { return TreeNode::totalNodes; }
private:
	TreeNode *parent;
	std::map<int, class TreeNode *> children;
	std::set<int> backtrack;
	static int totalNodes;
};

#endif /* __TREE_H__ */
