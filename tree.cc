#include "tree.h"
#include "action.h"

int TreeNode::totalNodes = 0;

TreeNode::TreeNode(TreeNode *par, ModelAction *act)
{
	TreeNode::totalNodes++;
	this->parent = par;
	if (!parent) {
		num_threads = 1;
	} else {
		num_threads = parent->num_threads;
		if (act && act->get_type() == THREAD_CREATE)
			num_threads++;
	}
}

TreeNode::~TreeNode() {
	std::map<int, class TreeNode *, std::less< int >, MyAlloc< std::pair< const int, class TreeNode * > > >::iterator it;

	for (it = children.begin(); it != children.end(); it++)
		delete it->second;
}

TreeNode * TreeNode::explore_child(ModelAction *act)
{
	TreeNode *n;
	std::set<int, std::less< int >, MyAlloc< int > >::iterator it;
	thread_id_t id = act->get_tid();
	int i = id_to_int(id);

	if (!hasBeenExplored(id)) {
		n = new TreeNode(this, act);
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

bool TreeNode::is_enabled(Thread *t)
{
	return id_to_int(t->get_id()) < num_threads;
}
