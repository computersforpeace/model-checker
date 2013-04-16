/** @file nodestack.h
 *  @brief Stack of operations for use in backtracking.
*/

#ifndef __NODESTACK_H__
#define __NODESTACK_H__

#include <cstddef>
#include <inttypes.h>

#include "mymemory.h"
#include "schedule.h"
#include "promise.h"
#include "stl-model.h"

class ModelAction;
class Thread;

struct fairness_info {
	unsigned int enabled_count;
	unsigned int turns;
	bool priority;
};

/**
 * @brief Types of read-from relations
 *
 * Our "may-read-from" set is composed of multiple types of reads, and we have
 * to iterate through all of them in the backtracking search. This enumeration
 * helps to identify which type of read-from we are currently observing.
 */
typedef enum {
	READ_FROM_PAST, /**< @brief Read from a prior, existing store */
	READ_FROM_PROMISE, /**< @brief Read from an existing promised future value */
	READ_FROM_FUTURE, /**< @brief Read from a newly-asserted future value */
	READ_FROM_NONE, /**< @brief A NULL state, which should not be reached */
} read_from_type_t;

#define YIELD_E 1
#define YIELD_D 2
#define YIELD_S 4
#define YIELD_P 8
#define YIELD_INDEX(tid1, tid2, num_threads) (tid1*num_threads+tid2)


/**
 * @brief A single node in a NodeStack
 *
 * Represents a single node in the NodeStack. Each Node is associated with up
 * to one action and up to one parent node. A node holds information
 * regarding the last action performed (the "associated action"), the thread
 * choices that have been explored (explored_children) and should be explored
 * (backtrack), and the actions that the last action may read from.
 */
class Node {
public:
	Node(const struct model_params *params, ModelAction *act, Node *par,
			int nthreads, Node *prevfairness);
	~Node();
	/* return true = thread choice has already been explored */
	bool has_been_explored(thread_id_t tid) const;
	/* return true = backtrack set is empty */
	bool backtrack_empty() const;

	void clear_backtracking();
	void explore_child(ModelAction *act, enabled_type_t *is_enabled);
	/* return false = thread was already in backtrack */
	bool set_backtrack(thread_id_t id);
	thread_id_t get_next_backtrack();
	bool is_enabled(Thread *t) const;
	bool is_enabled(thread_id_t tid) const;
	enabled_type_t enabled_status(thread_id_t tid) const;

	ModelAction * get_action() const { return action; }
	void set_uninit_action(ModelAction *act) { uninit_action = act; }
	ModelAction * get_uninit_action() const { return uninit_action; }

	bool has_priority(thread_id_t tid) const;
	void update_yield(Scheduler *);
	bool has_priority_over(thread_id_t tid, thread_id_t tid2) const;
	int get_num_threads() const { return num_threads; }
	/** @return the parent Node to this Node; that is, the action that
	 * occurred previously in the stack. */
	Node * get_parent() const { return parent; }

	read_from_type_t get_read_from_status();
	bool increment_read_from();
	bool read_from_empty() const;
	unsigned int read_from_size() const;

	void print_read_from_past();
	void add_read_from_past(const ModelAction *act);
	const ModelAction * get_read_from_past() const;
	const ModelAction * get_read_from_past(int i) const;
	int get_read_from_past_size() const;

	void add_read_from_promise(const ModelAction *reader);
	Promise * get_read_from_promise() const;
	Promise * get_read_from_promise(int i) const;
	int get_read_from_promise_size() const;

	bool add_future_value(struct future_value fv);
	struct future_value get_future_value() const;

	void set_promise(unsigned int i);
	bool get_promise(unsigned int i) const;
	bool increment_promise();
	bool promise_empty() const;
	void clear_promise_resolutions();

	enabled_type_t *get_enabled_array() {return enabled_array;}

	void set_misc_max(int i);
	int get_misc() const;
	bool increment_misc();
	bool misc_empty() const;

	void add_relseq_break(const ModelAction *write);
	const ModelAction * get_relseq_break() const;
	bool increment_relseq_break();
	bool relseq_break_empty() const;

	bool increment_behaviors();

	void print() const;

	MEMALLOC
private:
	void explore(thread_id_t tid);
	int get_yield_data(int tid1, int tid2) const;
	bool read_from_past_empty() const;
	bool increment_read_from_past();
	bool read_from_promise_empty() const;
	bool increment_read_from_promise();
	bool future_value_empty() const;
	bool increment_future_value();
	read_from_type_t read_from_status;
	const struct model_params * get_params() const { return params; }

	ModelAction * const action;

	const struct model_params * const params;

	/** @brief ATOMIC_UNINIT action which was created at this Node */
	ModelAction *uninit_action;

	Node * const parent;
	const int num_threads;
	ModelVector<bool> explored_children;
	ModelVector<bool> backtrack;
	ModelVector<struct fairness_info> fairness;
	int numBacktracks;
	enabled_type_t *enabled_array;

	/**
	 * The set of past ModelActions that this the action at this Node may
	 * read from. Only meaningful if this Node represents a 'read' action.
	 */
	ModelVector<const ModelAction *> read_from_past;
	unsigned int read_from_past_idx;

	ModelVector<const ModelAction *> read_from_promises;
	int read_from_promise_idx;

	ModelVector<struct future_value> future_values;
	int future_index;

	ModelVector<bool> resolve_promise;
	int resolve_promise_idx;

	ModelVector<const ModelAction *> relseq_break_writes;
	int relseq_break_index;

	int misc_index;
	int misc_max;
	int * yield_data;
};

typedef ModelVector<Node *> node_list_t;

/**
 * @brief A stack of nodes
 *
 * Holds a Node linked-list that can be used for holding backtracking,
 * may-read-from, and replay information. It is used primarily as a
 * stack-like structure, in that backtracking points and replay nodes are
 * only removed from the top (most recent).
 */
class NodeStack {
public:
	NodeStack();
	~NodeStack();

	void register_engine(const ModelExecution *exec);

	ModelAction * explore_action(ModelAction *act, enabled_type_t * is_enabled);
	Node * get_head() const;
	Node * get_next() const;
	void reset_execution();
	void pop_restofstack(int numAhead);
	int get_total_nodes() { return total_nodes; }

	void print() const;

	MEMALLOC
private:
	node_list_t node_list;

	const struct model_params * get_params() const;

	/** @brief The model-checker execution object */
	const ModelExecution *execution;

	/**
	 * @brief the index position of the current head Node
	 *
	 * This index is relative to node_list. The index should point to the
	 * current head Node. It is negative when the list is empty.
	 */
	int head_idx;

	int total_nodes;
};

#endif /* __NODESTACK_H__ */
