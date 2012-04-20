#include "threads.h"
#include "schedule.h"
#include "common.h"
#include "model.h"

void Scheduler::add_thread(Thread *t)
{
	DEBUG("thread %d\n", t->get_id());
	readyList.push_back(t);
}

Thread *Scheduler::next_thread(void)
{
	if (readyList.empty())
		return NULL;

	current = readyList.front();
	readyList.pop_front();

	return current;
}

Thread *Scheduler::get_current_thread(void)
{
	return current;
}
