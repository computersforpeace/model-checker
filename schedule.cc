#include "threads_internal.h"
#include "schedule.h"
#include "common.h"
#include "model.h"

void Scheduler::add_thread(Thread *t)
{
	DEBUG("thread %d\n", t->get_id());
	queue.push(t);
}

Thread *Scheduler::next_thread(void)
{
	if (queue.empty())
		return NULL;

	current = queue.front();
	queue.pop();

	return current;
}

Thread *Scheduler::get_current_thread(void)
{
	return current;
}
