#include "libthreads.h"
#include "schedule.h"
#include "common.h"
#include "model.h"

void Scheduler::add_thread(struct thread *t)
{
	DEBUG("thread %d\n", t->id);
	queue.push(t);
}

struct thread *Scheduler::next_thread(void)
{
	if (queue.empty())
		return NULL;

	current = queue.front();
	queue.pop();

	return current;
}

struct thread *Scheduler::get_current_thread(void)
{
	return current;
}
