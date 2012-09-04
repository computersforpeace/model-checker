/** @file model.h
 *  @brief Core model checker.
 */

#ifndef __MODEL_H__
#define __MODEL_H__

#include <list>
#include <vector>
#include <cstddef>
#include <ucontext.h>

#include "schedule.h"
#include "mymemory.h"
#include "libthreads.h"
#include "threads.h"
#include "action.h"
#include "clockvector.h"
#include "hashtable.h"

/* Forward declaration */
class NodeStack;
class CycleGraph;
class Promise;

/**
 * Model checker parameter structure. Holds run-time configuration options for
 * the model checker.
 */
struct model_params {
};

/** @brief The central structure for model-checking */
class ModelChecker {
public:
	ModelChecker(struct model_params params);
	~ModelChecker();

	/** @returns the context for the main model-checking system thread */
	ucontext_t * get_system_context() { return &system_context; }

	/** Prints an execution summary with trace information. */
	void print_summary();

	void add_thread(Thread *t);
	void remove_thread(Thread *t);
	Thread * get_thread(thread_id_t tid) { return thread_map->get(id_to_int(tid)); }

	thread_id_t get_next_id();
	int get_num_threads();
	modelclock_t get_next_seq_num();

	/** @return The currently executing Thread. */
	Thread * get_current_thread() { return scheduler->get_current_thread(); }

	int switch_to_master(ModelAction *act);
	ClockVector * get_cv(thread_id_t tid);
	bool next_execution();
	bool isfeasible();
	bool isfinalfeasible();
	void check_promises(ClockVector *old_cv, ClockVector * merge_cv);
	void get_release_seq_heads(ModelAction *act,
	                std::vector<const ModelAction *> *release_heads);

	void finish_execution();

	MEMALLOC
private:
	/** The scheduler to use: tracks the running/ready Threads */
	Scheduler *scheduler;

	int next_thread_id;
	modelclock_t used_sequence_numbers;
	int num_executions;

	const model_params params;

	/**
	 * Stores the ModelAction for the current thread action.  Call this
	 * immediately before switching from user- to system-context to pass
	 * data between them.
	 * @param act The ModelAction created by the user-thread action
	 */
	void set_current_action(ModelAction *act) { current_action = act; }
	void check_current_action();

	bool take_step();

	ModelAction * get_last_conflict(ModelAction *act);
	void set_backtracking(ModelAction *act);
	Thread * get_next_replay_thread();
	ModelAction * get_next_backtrack();
	void reset_to_initial_state();
	bool resolve_promises(ModelAction *curr);
	void compute_promises(ModelAction *curr);

	void add_action_to_lists(ModelAction *act);
	ModelAction * get_last_action(thread_id_t tid);
	ModelAction * get_parent_action(thread_id_t tid);
	ModelAction * get_last_seq_cst(const void *location);
	void build_reads_from_past(ModelAction *curr);
	ModelAction * process_rmw(ModelAction *curr);
	void post_r_modification_order(ModelAction *curr, const ModelAction *rf);
	bool r_modification_order(ModelAction *curr, const ModelAction *rf);
	bool w_modification_order(ModelAction *curr);
	bool release_seq_head(const ModelAction *rf,
	                std::vector<const ModelAction *> *release_heads) const;
	bool resolve_release_sequences(void *location);

	ModelAction *current_action;
	ModelAction *diverge;
	Thread *nextThread;

	ucontext_t system_context;
	action_list_t *action_trace;
	HashTable<int, Thread *, int> *thread_map;

	/** Per-object list of actions. Maps an object (i.e., memory location)
	 * to a trace of all actions performed on the object. */
	HashTable<const void *, action_list_t, uintptr_t, 4> *obj_map;

	HashTable<void *, std::vector<action_list_t>, uintptr_t, 4 > *obj_thrd_map;
	std::vector<Promise *> *promises;

	/**
	 * Collection of lists of objects that might synchronize with one or
	 * more release sequence. Release sequences might be determined lazily
	 * as promises are fulfilled and modification orders are established.
	 * This structure maps its lists by object location. Each ModelAction
	 * in the lists should be an acquire operation.
	 */
	HashTable<void *, std::list<ModelAction *>, uintptr_t, 4> *lazy_sync_with_release;

	std::vector<ModelAction *> *thrd_last_action;
	NodeStack *node_stack;
	ModelAction *next_backtrack;

	/**
	 * @brief The modification order graph
	 *
	 * A direceted acyclic graph recording observations of the modification
	 * order on all the atomic objects in the system. This graph should
	 * never contain any cycles, as that represents a violation of the
	 * memory model (total ordering). This graph really consists of many
	 * disjoint (unconnected) subgraphs, each graph corresponding to a
	 * separate ordering on a distinct object.
	 *
	 * The edges in this graph represent the "ordered before" relation,
	 * such that <tt>a --> b</tt> means <tt>a</tt> was ordered before
	 * <tt>b</tt>.
	 */
	CycleGraph *mo_graph;

	bool failed_promise;
};

extern ModelChecker *model;

#endif /* __MODEL_H__ */
