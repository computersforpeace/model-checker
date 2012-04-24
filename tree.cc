#include "tree.h"

int TreeNode::totalNodes = 0;

TreeNode::TreeNode(TreeNode *par)
{
	TreeNode::totalNodes++;
	this->parent = par;
}

TreeNode::~TreeNode() {
	std::map<tree_t, class TreeNode *>::iterator it;

	for (it = children.begin(); it != children.end(); it++)
		delete it->second;
}

TreeNode * TreeNode::exploreChild(tree_t id)
{
	TreeNode *n;
	std::set<tree_t >::iterator it;

	if (!hasBeenExplored(id)) {
		n = new TreeNode(this);
		children[id] = n;
	} else {
		n = children[id];
	}
	if ((it = backtrack.find(id)) != backtrack.end())
		backtrack.erase(it);

	return n;
}

int TreeNode::setBacktrack(tree_t id)
{
	if (backtrack.find(id) != backtrack.end())
		return 1;
	backtrack.insert(id);
	return 0;
}

tree_t TreeNode::getNextBacktrack()
{
	if (backtrack.empty())
		return TREE_T_NONE;
	return *backtrack.begin();
}

TreeNode * TreeNode::getRoot()
{
	if (parent)
		return parent->getRoot();
	return this;
}
