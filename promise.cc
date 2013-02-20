#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "promise.h"
#include "model.h"
#include "schedule.h"

/**
 * @brief Promise constructor
 * @param read The read which reads from a promised future value
 * @param fv The future value that is promised
 */
Promise::Promise(ModelAction *read, struct future_value fv) :
	num_available_threads(0),
	value(fv.value),
	expiration(fv.expiration),
	read(read),
	write(NULL)
{
	add_thread(fv.tid);
	eliminate_thread(read->get_tid());
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

/** @brief Print debug info about the Promise */
void Promise::print() const
{
	model_print("Promised value %#" PRIx64 ", read from thread %d, available threads to resolve: ", value, id_to_int(read->get_tid()));
	for (unsigned int i = 0; i < available_thread.size(); i++)
		if (available_thread[i])
			model_print("[%d]", i);
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
	return thread_is_available(act->get_tid()) && read->same_var(act);
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
