#include "model.h"
#include "schedule.h"

ModelChecker *model;

ModelChecker::ModelChecker()
{
	/* First thread created (system_thread) will have id 1 */
	this->used_thread_id = 0;
	/* Initialize default scheduler */
	this->scheduler = new DefaultScheduler();
}

ModelChecker::~ModelChecker()
{
	delete this->scheduler;
}

void ModelChecker::assign_id(struct thread *t)
{
	t->id = ++this->used_thread_id;
}

void ModelChecker::add_system_thread(struct thread *t)
{
	this->system_thread = t;
}
