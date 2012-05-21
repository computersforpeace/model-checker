#include "threads.h"
#include "schedule.h"
#include "common.h"
#include "model.h"

Scheduler::Scheduler() :
	current(NULL)
{
}

void Scheduler::add_thread(Thread *t)
{
	DEBUG("thread %d\n", t->get_id());
	readyList.push_back(t);
}

void Scheduler::remove_thread(Thread *t)
{
	if (current == t)
		current = NULL;
	else
		readyList.remove(t);
}

Thread * Scheduler::next_thread(void)
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

Thread * Scheduler::get_current_thread(void)
{
	return current;
}

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
