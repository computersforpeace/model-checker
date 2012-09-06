#include "threads.h"
#include "schedule.h"
#include "common.h"
#include "model.h"

/** Constructor */
Scheduler::Scheduler() :
	current(NULL)
{
}

/**
 * Add a Thread to the scheduler's ready list.
 * @param t The Thread to add
 */
void Scheduler::add_thread(Thread *t)
{
	DEBUG("thread %d\n", t->get_id());
	readyList.push_back(t);
}

/**
 * Remove a given Thread from the scheduler.
 * @param t The Thread to remove
 */
void Scheduler::remove_thread(Thread *t)
{
	if (current == t)
		current = NULL;
	else
		readyList.remove(t);
}

/**
 * Force one Thread to wait on another Thread. The "join" Thread should
 * eventually wake up the waiting Thread via Scheduler::wake.
 * @param wait The Thread that should wait
 * @param join The Thread on which we are waiting.
 */
void Scheduler::wait(Thread *wait, Thread *join)
{
	ASSERT(!join->is_complete());
	remove_thread(wait);
	join->push_wait_list(wait);
	wait->set_state(THREAD_BLOCKED);
}

/**
 * Wake a Thread up that was previously waiting (see Scheduler::wait)
 * @param t The Thread to wake up
 */
void Scheduler::wake(Thread *t)
{
	add_thread(t);
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
	if (t != NULL) {
		current = t;
		readyList.remove(t);
	} else if (readyList.empty()) {
		t = NULL;
	} else {
		t = readyList.front();
		current = t;
		readyList.pop_front();
	}

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
	DEBUG("Num. threads in ready list: %zu\n", readyList.size());

	std::list<Thread *, MyAlloc< Thread * > >::const_iterator it;
	for (it = readyList.begin(); it != readyList.end(); it++)
		DEBUG("In ready list: thread %d\n", (*it)->get_id());
}
