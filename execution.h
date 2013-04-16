/** @file execution.h
 *  @brief Model-checker core
 */

#ifndef __EXECUTION_H__
#define __EXECUTION_H__

#include <cstddef>
#include <inttypes.h>

#include "mymemory.h"
#include "hashtable.h"
#include "workqueue.h"
#include "config.h"
#include "modeltypes.h"
#include "stl-model.h"
#include "context.h"
#include "params.h"

/* Forward declaration */
class Node;
class NodeStack;
class CycleGraph;
class Promise;
class Scheduler;
class Thread;
class ClockVector;
struct model_snapshot_members;
class ModelChecker;
struct bug_message;

/** @brief Shorthand for a list of release sequence heads */
typedef ModelVector<const ModelAction *> rel_heads_list_t;
typedef SnapList<ModelAction *> action_list_t;

struct PendingFutureValue {
	PendingFutureValue(ModelAction *writer, ModelAction *reader) :
		writer(writer), reader(reader)
	{ }
	const ModelAction *writer;
	ModelAction *reader;
};

/** @brief Records information regarding a single pending release sequence */
struct release_seq {
	/** @brief The acquire operation */
	ModelAction *acquire;
	/** @brief The read operation that may read from a release sequence;
	 *  may be the same as acquire, or else an earlier action in the same
	 *  thread (i.e., when 'acquire' is a fence-acquire) */
	const ModelAction *read;
	/** @brief The head of the RMW chain from which 'read' reads; may be
	 *  equal to 'release' */
	const ModelAction *rf;
	/** @brief The head of the potential longest release sequence chain */
	const ModelAction *release;
	/** @brief The write(s) that may break the release sequence */
	SnapVector<const ModelAction *> writes;
};

/** @brief The central structure for model-checking */
class ModelExecution {
public:
	ModelExecution(struct model_params *params, Scheduler *scheduler, NodeStack *node_stack);
	~ModelExecution();

	Thread * take_step(ModelAction *curr);
	void fixup_release_sequences();

	void print_summary() const;
#if SUPPORT_MOD_ORDER_DUMP
	void dumpGraph(char *filename) const;
#endif

	void add_thread(Thread *t);
	Thread * get_thread(thread_id_t tid) const;
	Thread * get_thread(const ModelAction *act) const;
	int get_promise_number(const Promise *promise) const;

	bool is_enabled(Thread *t) const;
	bool is_enabled(thread_id_t tid) const;

	thread_id_t get_next_id();
	unsigned int get_num_threads() const;

	ClockVector * get_cv(thread_id_t tid) const;
	ModelAction * get_parent_action(thread_id_t tid) const;
	void check_promises_thread_disabled();
	bool isfeasibleprefix() const;

	action_list_t * get_actions_on_obj(void * obj, thread_id_t tid);
	ModelAction * get_last_action(thread_id_t tid) const;

	bool check_action_enabled(ModelAction *curr);

	bool assert_bug(const char *msg);
	bool have_bug_reports() const;
	SnapVector<bug_message *> * get_bugs() const;

	bool has_asserted() const;
	void set_assert();
	bool is_complete_execution() const;

	void print_infeasibility(const char *prefix) const;
	bool is_feasible_prefix_ignore_relseq() const;
	bool is_infeasible() const;
	bool is_deadlocked() const;
	bool too_many_steps() const;

	ModelAction * get_next_backtrack();

	action_list_t * get_action_trace() const { return action_trace; }

	void increment_execution_number() { execution_number++; }

	MEMALLOC
private:
	const model_params * const params;

	/** The scheduler to use: tracks the running/ready Threads */
	Scheduler * const scheduler;

	bool sleep_can_read_from(ModelAction *curr, const ModelAction *write);
	bool thin_air_constraint_may_allow(const ModelAction *writer, const ModelAction *reader) const;
	bool mo_may_allow(const ModelAction *writer, const ModelAction *reader);
	bool promises_may_allow(const ModelAction *writer, const ModelAction *reader) const;
	void set_bad_synchronization();
	bool promises_expired() const;
	bool should_wake_up(const ModelAction *curr, const Thread *thread) const;
	void wake_up_sleeping_actions(ModelAction *curr);
	modelclock_t get_next_seq_num();

	bool next_execution();
	ModelAction * check_current_action(ModelAction *curr);
	bool initialize_curr_action(ModelAction **curr);
	bool process_read(ModelAction *curr);
	bool process_write(ModelAction *curr);
	bool process_fence(ModelAction *curr);
	bool process_mutex(ModelAction *curr);
	bool process_thread_action(ModelAction *curr);
	void process_relseq_fixup(ModelAction *curr, work_queue_t *work_queue);
	bool read_from(ModelAction *act, const ModelAction *rf);
	bool synchronize(const ModelAction *first, ModelAction *second);

	template <typename T>
	bool check_recency(ModelAction *curr, const T *rf) const;

	template <typename T, typename U>
	bool should_read_instead(const ModelAction *curr, const T *rf, const U *other_rf) const;

	ModelAction * get_last_fence_conflict(ModelAction *act) const;
	ModelAction * get_last_conflict(ModelAction *act) const;
	void set_backtracking(ModelAction *act);
	bool set_latest_backtrack(ModelAction *act);
	Promise * pop_promise_to_resolve(const ModelAction *curr);
	bool resolve_promise(ModelAction *curr, Promise *promise);
	void compute_promises(ModelAction *curr);
	void compute_relseq_breakwrites(ModelAction *curr);

	void check_promises(thread_id_t tid, ClockVector *old_cv, ClockVector *merge_cv);
	void mo_check_promises(const ModelAction *act, bool is_read_check);
	void thread_blocking_check_promises(Thread *blocker, Thread *waiting);

	void check_curr_backtracking(ModelAction *curr);
	void add_action_to_lists(ModelAction *act);
	ModelAction * get_last_fence_release(thread_id_t tid) const;
	ModelAction * get_last_seq_cst_write(ModelAction *curr) const;
	ModelAction * get_last_seq_cst_fence(thread_id_t tid, const ModelAction *before_fence) const;
	ModelAction * get_last_unlock(ModelAction *curr) const;
	void build_may_read_from(ModelAction *curr);
	ModelAction * process_rmw(ModelAction *curr);

	template <typename rf_type>
	bool r_modification_order(ModelAction *curr, const rf_type *rf);

	bool w_modification_order(ModelAction *curr, ModelVector<ModelAction *> *send_fv);
	void get_release_seq_heads(ModelAction *acquire, ModelAction *read, rel_heads_list_t *release_heads);
	bool release_seq_heads(const ModelAction *rf, rel_heads_list_t *release_heads, struct release_seq *pending) const;
	bool resolve_release_sequences(void *location, work_queue_t *work_queue);
	void add_future_value(const ModelAction *writer, ModelAction *reader);

	ModelAction * get_uninitialized_action(const ModelAction *curr) const;

	action_list_t * const action_trace;
	HashTable<int, Thread *, int> * const thread_map;

	/** Per-object list of actions. Maps an object (i.e., memory location)
	 * to a trace of all actions performed on the object. */
	HashTable<const void *, action_list_t *, uintptr_t, 4> * const obj_map;

	/** Per-object list of actions. Maps an object (i.e., memory location)
	 * to a trace of all actions performed on the object. */
	HashTable<const void *, action_list_t *, uintptr_t, 4> * const condvar_waiters_map;

	HashTable<void *, SnapVector<action_list_t> *, uintptr_t, 4 > * const obj_thrd_map;
	SnapVector<Promise *> * const promises;
	SnapVector<struct PendingFutureValue> * const futurevalues;

	/**
	 * List of pending release sequences. Release sequences might be
	 * determined lazily as promises are fulfilled and modification orders
	 * are established. Each entry in the list may only be partially
	 * filled, depending on its pending status.
	 */
	SnapVector<struct release_seq *> * const pending_rel_seqs;

	SnapVector<ModelAction *> * const thrd_last_action;
	SnapVector<ModelAction *> * const thrd_last_fence_release;
	NodeStack * const node_stack;

	/** A special model-checker Thread; used for associating with
	 *  model-checker-related ModelAcitons */
	Thread *model_thread;

	/** Private data members that should be snapshotted. They are grouped
	 * together for efficiency and maintainability. */
	struct model_snapshot_members * const priv;

	/**
	 * @brief The modification order graph
	 *
	 * A directed acyclic graph recording observations of the modification
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
	CycleGraph * const mo_graph;

	int execution_number;

	Thread * action_select_next_thread(const ModelAction *curr) const;
};

#endif /* __EXECUTION_H__ */
