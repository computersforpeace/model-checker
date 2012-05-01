#include "tree.h"

int TreeNode::totalNodes = 0;

TreeNode::TreeNode(TreeNode *par)
{
	TreeNode::totalNodes++;
	this->parent = par;
}

TreeNode::~TreeNode() {
	std::map<int, class TreeNode *>::iterator it;

	for (it = children.begin(); it != children.end(); it++)
		delete it->second;
}

TreeNode * TreeNode::exploreChild(thread_id_t id)
{
	TreeNode *n;
	std::set<int>::iterator it;
	int i = id_to_int(id);

	if (!hasBeenExplored(id)) {
		n = new TreeNode(this);
		children[i] = n;
	} else {
		n = children[i];
	}
	if ((it = backtrack.find(i)) != backtrack.end())
		backtrack.erase(it);

	return n;
}

int TreeNode::setBacktrack(thread_id_t id)
{
	int i = id_to_int(id);
	if (backtrack.find(i) != backtrack.end())
		return 1;
	backtrack.insert(i);
	return 0;
}

thread_id_t TreeNode::getNextBacktrack()
{
	if (backtrack.empty())
		return THREAD_ID_T_NONE;
	return int_to_id(*backtrack.begin());
}

TreeNode * TreeNode::getRoot()
{
	if (parent)
		return parent->getRoot();
	return this;
}
