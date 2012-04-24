#ifndef __MODEL_H__
#define __MODEL_H__

#include <list>
#include <map>
#include <cstddef>

#include "schedule.h"
#include "libthreads.h"
#include "libatomic.h"
#include "threads.h"
#include "tree.h"

#define VALUE_NONE -1

typedef enum action_type {
	THREAD_CREATE,
	THREAD_YIELD,
	THREAD_JOIN,
	ATOMIC_READ,
	ATOMIC_WRITE
} action_type_t;

typedef std::list<class ModelAction *> action_list_t;

class ModelAction {
public:
	ModelAction(action_type_t type, memory_order order, void *loc, int value);
	void print(void);

	thread_id_t get_tid() { return tid; }
	action_type get_type() { return type; }
	memory_order get_mo() { return order; }
	void *get_location() { return location; }

	TreeNode *get_node() { return node; }
	void set_node(TreeNode *n) { node = n; }
private:
	action_type type;
	memory_order order;
	void *location;
	thread_id_t tid;
	int value;
	TreeNode *node;
};

class Backtrack {
public:
	Backtrack(ModelAction *d, action_list_t *t) {
		diverge = d;
		actionTrace = t;
		iter = actionTrace->begin();
	}
	ModelAction *get_diverge() { return diverge; }
	action_list_t *get_trace() { return actionTrace; }
	void advance_state() { iter++; }
	ModelAction *get_state() {
		return iter == actionTrace->end() ? NULL : *iter;
	}
private:
	ModelAction *diverge;
	action_list_t *actionTrace;
	/* points to position in actionTrace as we replay */
	action_list_t::iterator iter;
};

class ModelChecker {
public:
	ModelChecker();
	~ModelChecker();
	class Scheduler *scheduler;
	Thread *system_thread;

	void add_system_thread(Thread *t);

	void set_current_action(ModelAction *act) { current_action = act; }
	void check_current_action(void);
	void print_trace(void);
	Thread *schedule_next_thread();

	int add_thread(Thread *t);
	Thread *get_thread(thread_id_t tid) { return thread_map[tid]; }

	void assign_id(Thread *t);

	int switch_to_master(ModelAction *act);

	bool next_execution();
private:
	int used_thread_id;
	int num_executions;

	ModelAction *get_last_conflict(ModelAction *act);
	void set_backtracking(ModelAction *act);
	thread_id_t advance_backtracking_state();
	thread_id_t get_next_replay_thread();

	class ModelAction *current_action;
	Backtrack *exploring;
	thread_id_t nextThread;

	action_list_t *action_trace;
	std::map<thread_id_t, class Thread *> thread_map;
	class TreeNode *rootNode, *currentNode;
	std::list<class Backtrack *> backtrack_list;
};

extern ModelChecker *model;

int thread_switch_to_master(ModelAction *act);

#endif /* __MODEL_H__ */
