#include <string.h>
#include <stdlib.h>

#include "threads-model.h"
#include "schedule.h"
#include "common.h"
#include "model.h"
#include "nodestack.h"

/** Constructor */
Scheduler::Scheduler() :
	enabled(NULL),
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
		if (enabled != NULL) {
			memcpy(new_enabled, enabled, enabled_len*sizeof(enabled_type_t));
			snapshot_free(enabled);
		}
		enabled=new_enabled;
		enabled_len=threadid+1;
	}
	enabled[threadid]=enabled_status;
	if (enabled_status == THREAD_DISABLED)
		model->check_promises_thread_disabled();
}

/**
 * @brief Check if a Thread is currently enabled
 *
 * Check if a Thread is currently enabled. "Enabled" includes both
 * THREAD_ENABLED and THREAD_SLEEP_SET.
 * @param t The Thread to check
 * @return True if the Thread is currently enabled
 */
bool Scheduler::is_enabled(Thread *t) const
{
	return is_enabled(t->get_id());
}

/**
 * @brief Check if a Thread is currently enabled
 *
 * Check if a Thread is currently enabled. "Enabled" includes both
 * THREAD_ENABLED and THREAD_SLEEP_SET.
 * @param tid The ID of the Thread to check
 * @return True if the Thread is currently enabled
 */
bool Scheduler::is_enabled(thread_id_t tid) const
{
	int i = id_to_int(tid);
	return (i >= enabled_len) ? false : (enabled[i] != THREAD_DISABLED);
}

enabled_type_t Scheduler::get_enabled(Thread *t) {
	int id = id_to_int(t->get_id());
	ASSERT(id<enabled_len);
	return enabled[id];
}

void Scheduler::update_sleep_set(Node *n) {
	enabled_type_t *enabled_array=n->get_enabled_array();
	for(int i=0;i<enabled_len;i++) {
		if (enabled_array[i]==THREAD_SLEEP_SET) {
			enabled[i]=THREAD_SLEEP_SET;
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
	ASSERT(!t->is_model_thread());
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
	ASSERT(!t->is_model_thread());
	set_enabled(t, THREAD_ENABLED);
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
		bool have_enabled_thread_with_priority=false;
		Node *n=model->get_curr_node();

		for(int i=0;i<enabled_len;i++) {
			thread_id_t tid=int_to_id(i);
			if (n->has_priority(tid)) {
				//Have a thread with priority
				if (enabled[i]!=THREAD_DISABLED)
					have_enabled_thread_with_priority=true;
			}
		}

		while(true) {
			curr_thread_index = (curr_thread_index+1) % enabled_len;
			thread_id_t curr_tid=int_to_id(curr_thread_index);
			if (enabled[curr_thread_index]==THREAD_ENABLED&&
					(!have_enabled_thread_with_priority||n->has_priority(curr_tid))) {
				t = model->get_thread(curr_tid);
				break;
			}
			if (curr_thread_index == old_curr_thread) {
				print();
				return NULL;
			}
		}
	} else if (t->is_model_thread()) {
		/* model-checker threads never run */
		t = NULL;
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
	ASSERT(!current || !current->is_model_thread());
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
