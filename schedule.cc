#include <string.h>
#include <stdlib.h>

#include "threads.h"
#include "schedule.h"
#include "common.h"
#include "model.h"
#include "nodestack.h"

/** Constructor */
Scheduler::Scheduler() :
	is_enabled(NULL),
	enabled_len(0),
	curr_thread_index(0),
	current(NULL)
{
}

void Scheduler::set_enabled(Thread *t, enabled_type_t enabled_status) {
	int threadid=id_to_int(t->get_id());
	if (threadid>=enabled_len) {
		enabled_type_t *new_enabled = (enabled_type_t *)snapshot_malloc(sizeof(enabled_type_t) * (threadid + 1));
		memset(&new_enabled[enabled_len], 0, (threadid+1-enabled_len)*sizeof(enabled_type_t));
		if (is_enabled != NULL) {
			memcpy(new_enabled, is_enabled, enabled_len*sizeof(enabled_type_t));
			snapshot_free(is_enabled);
		}
		is_enabled=new_enabled;
		enabled_len=threadid+1;
	}
	is_enabled[threadid]=enabled_status;
}

enabled_type_t Scheduler::get_enabled(Thread *t) {
	return is_enabled[id_to_int(t->get_id())];
}

void Scheduler::update_sleep_set(Node *n) {
	enabled_type_t *enabled_array=n->get_enabled_array();
	for(int i=0;i<enabled_len;i++) {
		if (enabled_array[i]==THREAD_SLEEP_SET) {
			is_enabled[i]=THREAD_SLEEP_SET;
		}
	}
}

/**
 * Add a Thread to the sleep set.
 * @param t The Thread to add
 */
void Scheduler::add_sleep(Thread *t)
{
	DEBUG("thread %d\n", id_to_int(t->get_id()));
	set_enabled(t, THREAD_SLEEP_SET);
}

/**
 * Remove a Thread from the sleep set.
 * @param t The Thread to remove
 */
void Scheduler::remove_sleep(Thread *t)
{
	DEBUG("thread %d\n", id_to_int(t->get_id()));
	set_enabled(t, THREAD_ENABLED);
}

/**
 * Add a Thread to the scheduler's ready list.
 * @param t The Thread to add
 */
void Scheduler::add_thread(Thread *t)
{
	DEBUG("thread %d\n", id_to_int(t->get_id()));
	set_enabled(t, THREAD_ENABLED);
}

/**
 * Remove a given Thread from the scheduler.
 * @param t The Thread to remove
 */
void Scheduler::remove_thread(Thread *t)
{
	if (current == t)
		current = NULL;
	set_enabled(t, THREAD_DISABLED);
}

/**
 * Prevent a Thread from being scheduled. The sleeping Thread should be
 * re-awoken via Scheduler::wake.
 * @param thread The Thread that should sleep
 */
void Scheduler::sleep(Thread *t)
{
	set_enabled(t, THREAD_DISABLED);
	t->set_state(THREAD_BLOCKED);
}

/**
 * Wake a Thread up that was previously waiting (see Scheduler::wait)
 * @param t The Thread to wake up
 */
void Scheduler::wake(Thread *t)
{
	set_enabled(t, THREAD_DISABLED);
	t->set_state(THREAD_READY);
}

/**
 * Select a Thread. This implementation defaults to round-robin, if a
 * thread is not already provided.
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
			if (is_enabled[curr_thread_index]==THREAD_ENABLED) {
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
		DEBUG("Current thread: %d\n", id_to_int(current->get_id()));
	else
		DEBUG("No current thread\n");
}
