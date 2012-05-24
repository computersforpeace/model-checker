#ifndef __MODEL_H__
#define __MODEL_H__

#include <list>
#include <map>
#include <vector>
#include <cstddef>
#include <ucontext.h>

#include "schedule.h"
#include "mymemory.h"
#include <utility>
#include "libthreads.h"
#include "libatomic.h"
#include "threads.h"
#include "action.h"

/* Forward declaration */
class NodeStack;

class ModelChecker {
public:
	ModelChecker();
	~ModelChecker();
	class Scheduler *scheduler;

	void set_system_context(ucontext_t *ctxt) { system_context = ctxt; }
	ucontext_t * get_system_context(void) { return system_context; }

	void set_current_action(ModelAction *act) { current_action = act; }
	void check_current_action(void);
	void print_summary(void);
	Thread * schedule_next_thread();

	int add_thread(Thread *t);
	void remove_thread(Thread *t);
	Thread * get_thread(thread_id_t tid) { return thread_map[id_to_int(tid)]; }

	thread_id_t get_next_id();
	int get_next_seq_num();

	int switch_to_master(ModelAction *act);

	bool next_execution();

	MEMALLOC
private:
	int next_thread_id;
	int used_sequence_numbers;
	int num_executions;

	ModelAction * get_last_conflict(ModelAction *act);
	void set_backtracking(ModelAction *act);
	thread_id_t get_next_replay_thread();
	ModelAction * get_next_backtrack();
	void reset_to_initial_state();

	void print_list(action_list_t *list);

	ModelAction *current_action;
	ModelAction *diverge;
	thread_id_t nextThread;

	ucontext_t *system_context;
	action_list_t *action_trace;
	std::map<int, class Thread *> thread_map;
	class NodeStack *node_stack;
	ModelAction *next_backtrack;
};

extern ModelChecker *model;

#endif /* __MODEL_H__ */
