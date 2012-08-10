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
 * Remove one Thread from the scheduler. This implementation performs FIFO.
 * @return The next Thread to run
 */
Thread * Scheduler::next_thread()
{
	Thread *t = model->schedule_next_thread();

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
void Scheduler::print()
{
	if (current)
		DEBUG("Current thread: %d\n", current->get_id());
	else
		DEBUG("No current thread\n");
	DEBUG("Num. threads in ready list: %zu\n", readyList.size());

	std::list<Thread *, MyAlloc< Thread * > >::iterator it;
	for (it = readyList.begin(); it != readyList.end(); it++)
		DEBUG("In ready list: thread %d\n", (*it)->get_id());
}
