#include "threads.h"
#include "schedule.h"
#include "common.h"
#include "model.h"

/** Constructor */
Scheduler::Scheduler() :
	is_enabled(NULL),
	enabled_len(0),
	curr_thread_index(0),
	current(NULL)
{
}

void Scheduler::set_enabled(Thread *t, bool enabled_status) {
	int threadid=id_to_int(t->get_id());
	if (threadid>=enabled_len) {
		bool * new_enabled=(bool *)malloc(sizeof(bool)*(threadid+1));
		memset(&new_enabled[enabled_len], 0, (threadid+1-enabled_len)*sizeof(bool));
		if (is_enabled != NULL) {
			memcpy(new_enabled, is_enabled, enabled_len*sizeof(bool));
			free(is_enabled);
		}
		is_enabled=new_enabled;
		enabled_len=threadid+1;
	}
	is_enabled[threadid]=enabled_status;
}

/**
 * Add a Thread to the scheduler's ready list.
 * @param t The Thread to add
 */
void Scheduler::add_thread(Thread *t)
{
	DEBUG("thread %d\n", t->get_id());
	set_enabled(t, true);
}

/**
 * Remove a given Thread from the scheduler.
 * @param t The Thread to remove
 */
void Scheduler::remove_thread(Thread *t)
{
	if (current == t)
		current = NULL;
	set_enabled(t, false);
}

/**
 * Prevent a Thread from being scheduled. The sleeping Thread should be
 * re-awoken via Scheduler::wake.
 * @param thread The Thread that should sleep
 */
void Scheduler::sleep(Thread *t)
{
	set_enabled(t, false);
	t->set_state(THREAD_BLOCKED);
}

/**
 * Wake a Thread up that was previously waiting (see Scheduler::wait)
 * @param t The Thread to wake up
 */
void Scheduler::wake(Thread *t)
{
	set_enabled(t, true);
	t->set_state(THREAD_READY);
}

/**
 * Remove one Thread from the scheduler. This implementation defaults to FIFO,
 * if a thread is not already provided.
 *
 * @param t Thread to run, if chosen by an external entity (e.g.,
 * ModelChecker). May be NULL to indicate no external choice.
 * @return The next Thread to run
 */
Thread * Scheduler::next_thread(Thread *t)
{
	if ( t == NULL ) {
		int old_curr_thread = curr_thread_index;
		while(true) {
			curr_thread_index = (curr_thread_index+1) % enabled_len;
			if (is_enabled[curr_thread_index]) {
				t = model->get_thread(int_to_id(curr_thread_index));
				break;
			}
			if (curr_thread_index == old_curr_thread) {
				print();
				return NULL;
			}
		}
	} else {
		curr_thread_index = id_to_int(t->get_id());
	}

	current = t;
	print();
	return t;
}

/**
 * @return The currently-running Thread
 */
Thread * Scheduler::get_current_thread() const
{
	return current;
}

/**
 * Print debugging information about the current state of the scheduler. Only
 * prints something if debugging is enabled.
 */
void Scheduler::print() const
{
	if (current)
		DEBUG("Current thread: %d\n", current->get_id());
	else
		DEBUG("No current thread\n");
}
