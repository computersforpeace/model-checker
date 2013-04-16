#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "promise.h"
#include "execution.h"
#include "schedule.h"
#include "action.h"
#include "threads-model.h"

/**
 * @brief Promise constructor
 * @param execution The execution which is creating this Promise
 * @param read The read which reads from a promised future value
 * @param fv The future value that is promised
 */
Promise::Promise(const ModelExecution *execution, ModelAction *read, struct future_value fv) :
	execution(execution),
	num_available_threads(0),
	fv(fv),
	readers(1, read),
	write(NULL)
{
	add_thread(fv.tid);
	eliminate_thread(read->get_tid());
}

/**
 * Add a reader that reads from this Promise. Must be added in an order
 * consistent with execution order.
 *
 * @param reader The ModelAction that reads from this promise. Must be a read.
 * @return True if this new reader has invalidated the promise; false otherwise
 */
bool Promise::add_reader(ModelAction *reader)
{
	readers.push_back(reader);
	return eliminate_thread(reader->get_tid());
}

/**
 * Access a reader that read from this Promise. Readers must be inserted in
 * order by execution order, so they can be returned in this order.
 *
 * @param i The index of the reader to return
 * @return The i'th reader of this Promise
 */
ModelAction * Promise::get_reader(unsigned int i) const
{
	return i < readers.size() ? readers[i] : NULL;
}

/**
 * Eliminate a thread which no longer can satisfy this promise. Once all
 * enabled threads have been eliminated, this promise is unresolvable.
 *
 * @param tid The thread ID of the thread to eliminate
 * @return True, if this elimination has invalidated the promise; false
 * otherwise
 */
bool Promise::eliminate_thread(thread_id_t tid)
{
	unsigned int id = id_to_int(tid);
	if (!thread_is_available(tid))
		return false;

	available_thread[id] = false;
	num_available_threads--;
	return has_failed();
}

/**
 * Add a thread which may resolve this promise
 *
 * @param tid The thread ID
 */
void Promise::add_thread(thread_id_t tid)
{
	unsigned int id = id_to_int(tid);
	if (id >= available_thread.size())
		available_thread.resize(id + 1, false);
	if (!available_thread[id]) {
		available_thread[id] = true;
		num_available_threads++;
	}
}

/**
 * Check if a thread is available for resolving this promise. That is, the
 * thread must have been previously marked for resolving this promise, and it
 * cannot have been eliminated due to synchronization, etc.
 *
 * @param tid Thread ID of the thread to check
 * @return True if the thread is available; false otherwise
 */
bool Promise::thread_is_available(thread_id_t tid) const
{
	unsigned int id = id_to_int(tid);
	if (id >= available_thread.size())
		return false;
	return available_thread[id];
}

/**
 * @brief Get an upper bound on the number of available threads
 *
 * Gets an upper bound on the number of threads in the available threads set,
 * useful for iterating over "thread_is_available()".
 *
 * @return The upper bound
 */
unsigned int Promise::max_available_thread_idx() const
{
	return available_thread.size();
}

/** @brief Print debug info about the Promise */
void Promise::print() const
{
	model_print("Promised value %#" PRIx64 ", first read from thread %d, available threads to resolve: ",
			fv.value, id_to_int(get_reader(0)->get_tid()));
	bool failed = true;
	for (unsigned int i = 0; i < available_thread.size(); i++)
		if (available_thread[i]) {
			model_print("[%d]", i);
			failed = false;
		}
	if (failed)
		model_print("(none)");
	model_print("\n");
}

/**
 * Check if this promise has failed. A promise can fail when all threads which
 * could possibly satisfy the promise have been eliminated.
 *
 * @return True, if this promise has failed; false otherwise
 */
bool Promise::has_failed() const
{
	return num_available_threads == 0;
}

/**
 * @brief Check if an action's thread and location are compatible for resolving
 * this promise
 * @param act The action to check against
 * @return True if we are compatible; false otherwise
 */
bool Promise::is_compatible(const ModelAction *act) const
{
	return thread_is_available(act->get_tid()) && get_reader(0)->same_var(act);
}

/**
 * @brief Check if an action's thread and location are compatible for resolving
 * this promise, and that the promise is thread-exclusive
 * @param act The action to check against
 * @return True if we are compatible and exclusive; false otherwise
 */
bool Promise::is_compatible_exclusive(const ModelAction *act) const
{
	return get_num_available_threads() == 1 && is_compatible(act);
}

/**
 * @brief Check if a store's value matches this Promise
 * @param write The store to check
 * @return True if the store's written value matches this Promise
 */
bool Promise::same_value(const ModelAction *write) const
{
	return get_value() == write->get_write_value();
}

/**
 * @brief Check if a ModelAction's location matches this Promise
 * @param act The ModelAction to check
 * @return True if the action's location matches this Promise
 */
bool Promise::same_location(const ModelAction *act) const
{
	return get_reader(0)->same_var(act);
}

/** @brief Get this Promise's index within the execution's promise array */
int Promise::get_index() const
{
	return execution->get_promise_number(this);
}
