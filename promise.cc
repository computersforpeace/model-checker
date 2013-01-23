#include "promise.h"
#include "model.h"
#include "schedule.h"

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
	if (id >= eliminated_thread.size())
		eliminated_thread.resize(id + 1, false);
	if (eliminated_thread[id])
		return false;

	eliminated_thread[id] = true;
	return has_failed();
}

/**
 * Check if this promise has failed. A promise can fail when all threads which
 * could possibly satisfy the promise have been eliminated.
 *
 * @return True, if this promise has failed; false otherwise
 */
bool Promise::has_failed() const
{
	unsigned int size = eliminated_thread.size();
	int promise_tid = id_to_int(read->get_tid());
	for (unsigned int i = 1; i < model->get_num_threads(); i++) {
		if ((i >= size || !eliminated_thread[i]) && ((int)i != promise_tid) && model->is_enabled(int_to_id(i))) {
			return false;
		}
	}
	return true;
}
