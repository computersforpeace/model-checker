#include "model.h"
#include "schedule.h"
#include <stdlib.h>
#include <string.h>

ModelChecker *model;

ModelChecker::ModelChecker()
{
	/* First thread created (system_thread) will have id 1 */
	this->used_thread_id = 0;

	scheduler_init(this);
}

ModelChecker::~ModelChecker()
{
	struct scheduler *sched = model->scheduler;

	if (sched->exit)
		sched->exit();
	free(sched);
}

void ModelChecker::assign_id(struct thread *t)
{
	t->id = ++this->used_thread_id;
}

void ModelChecker::add_system_thread(struct thread *t)
{
	model->system_thread = t;
}
