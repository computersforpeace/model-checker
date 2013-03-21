/** @file model.h
 *  @brief Core model checker.
 */

#ifndef __MODEL_H__
#define __MODEL_H__

#include <cstddef>
#include <ucontext.h>
#include <inttypes.h>

#include "mymemory.h"
#include "hashtable.h"
#include "workqueue.h"
#include "config.h"
#include "modeltypes.h"
#include "stl-model.h"

/* Forward declaration */
class Node;
class NodeStack;
class CycleGraph;
class Promise;
class Scheduler;
class Thread;
class ClockVector;
struct model_snapshot_members;

/** @brief Shorthand for a list of release sequence heads */
typedef ModelVector<const ModelAction *> rel_heads_list_t;

typedef SnapList<ModelAction *> action_list_t;

/**
 * Model checker parameter structure. Holds run-time configuration options for
 * the model checker.
 */
struct model_params {
	int maxreads;
	int maxfuturedelay;
	bool yieldon;
	unsigned int fairwindow;
	unsigned int enabledcount;
	unsigned int bound;
	unsigned int uninitvalue;

	/** @brief Maximum number of future values that can be sent to the same
	 *  read */
	int maxfuturevalues;

	/** @brief Only generate a new future value/expiration pair if the
	 *  expiration time exceeds the existing one by more than the slop
	 *  value */
	unsigned int expireslop;

	/** @brief Verbosity (0 = quiet; 1 = noisy) */
	int verbose;

	/** @brief Command-line argument count to pass to user program */
	int argc;

	/** @brief Command-line arguments to pass to user program */
	char **argv;
};

/** @brief Model checker execution stats */
struct execution_stats {
	int num_total; /**< @brief Total number of executions */
	int num_infeasible; /**< @brief Number of infeasible executions */
	int num_buggy_executions; /** @brief Number of buggy executions */
	int num_complete; /**< @brief Number of feasible, non-buggy, complete executions */
	int num_redundant; /**< @brief Number of redundant, aborted executions */
};

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
class ModelChecker {
public:
	ModelChecker(struct model_params params);
	~ModelChecker();

	void run();

	/** @returns the context for the main model-checking system thread */
	ucontext_t * get_system_context() { return &system_context; }

	void print_summary() const;
#if SUPPORT_MOD_ORDER_DUMP
	void dumpGraph(char *filename) const;
#endif

	Thread * get_thread(thread_id_t tid) const;
	Thread * get_thread(const ModelAction *act) const;
	int get_promise_number(const Promise *promise) const;

	bool is_enabled(Thread *t) const;
	bool is_enabled(thread_id_t tid) const;

	thread_id_t get_next_id();
	unsigned int get_num_threads() const;
	Thread * get_current_thread() const;

	void switch_from_master(Thread *thread);
	uint64_t switch_to_master(ModelAction *act);
	ClockVector * get_cv(thread_id_t tid) const;
	ModelAction * get_parent_action(thread_id_t tid) const;
	void check_promises_thread_disabled();
	void check_promises(thread_id_t tid, ClockVector *old_cv, ClockVector *merge_cv);
	bool isfeasibleprefix() const;

	bool assert_bug(const char *msg);
	void assert_user_bug(const char *msg);

	const model_params params;
	Node * get_curr_node() const;

	MEMALLOC
private:
	/** The scheduler to use: tracks the running/ready Threads */
	Scheduler * const scheduler;

	void add_thread(Thread *t);

	bool sleep_can_read_from(ModelAction *curr, const ModelAction *write);
	bool thin_air_constraint_may_allow(const ModelAction *writer, const ModelAction *reader) const;
	bool mo_may_allow(const ModelAction *writer, const ModelAction *reader);
	bool has_asserted() const;
	void set_assert();
	void set_bad_synchronization();
	bool promises_expired() const;
	void execute_sleep_set();
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
	bool check_action_enabled(ModelAction *curr);

	Thread * take_step(ModelAction *curr);

	template <typename T>
	bool check_recency(ModelAction *curr, const T *rf) const;

	template <typename T, typename U>
	bool should_read_instead(const ModelAction *curr, const T *rf, const U *other_rf) const;

	ModelAction * get_last_fence_conflict(ModelAction *act) const;
	ModelAction * get_last_conflict(ModelAction *act) const;
	void set_backtracking(ModelAction *act);
	Thread * action_select_next_thread(const ModelAction *curr) const;
	Thread * get_next_thread();
	bool set_latest_backtrack(ModelAction *act);
	ModelAction * get_next_backtrack();
	void reset_to_initial_state();
	Promise * pop_promise_to_resolve(const ModelAction *curr);
	bool resolve_promise(ModelAction *curr, Promise *promise);
	void compute_promises(ModelAction *curr);
	void compute_relseq_breakwrites(ModelAction *curr);

	void mo_check_promises(const ModelAction *act, bool is_read_check);
	void thread_blocking_check_promises(Thread *blocker, Thread *waiting);

	void check_curr_backtracking(ModelAction *curr);
	void add_action_to_lists(ModelAction *act);
	ModelAction * get_last_action(thread_id_t tid) const;
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

	ModelAction *diverge;
	ModelAction *earliest_diverge;

	ucontext_t system_context;
	action_list_t * const action_trace;
	HashTable<int, Thread *, int> * const thread_map;

	/** Per-object list of actions. Maps an object (i.e., memory location)
	 * to a trace of all actions performed on the object. */
	HashTable<const void *, action_list_t *, uintptr_t, 4> * const obj_map;

	/** Per-object list of actions. Maps an object (i.e., memory location)
	 * to a trace of all actions performed on the object. */
	HashTable<const void *, action_list_t *, uintptr_t, 4> * const lock_waiters_map;

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

	/** Private data members that should be snapshotted. They are grouped
	 * together for efficiency and maintainability. */
	struct model_snapshot_members * const priv;

	/** A special model-checker Thread; used for associating with
	 *  model-checker-related ModelAcitons */
	Thread *model_thread;

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

	/** @brief The cumulative execution stats */
	struct execution_stats stats;
	void record_stats();

	void print_infeasibility(const char *prefix) const;
	bool is_feasible_prefix_ignore_relseq() const;
	bool is_infeasible() const;
	bool is_deadlocked() const;
	bool is_circular_wait(const Thread *t) const;
	bool is_complete_execution() const;
	bool have_bug_reports() const;
	void print_bugs() const;
	void print_execution(bool printbugs) const;
	void print_stats() const;

	friend void user_main_wrapper();
};

extern ModelChecker *model;

#endif /* __MODEL_H__ */
