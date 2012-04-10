#include <stdio.h>

#include "model.h"
#include "schedule.h"
#include "common.h"

ModelChecker *model;

ModelChecker::ModelChecker()
{
	/* First thread created (system_thread) will have id 1 */
	this->used_thread_id = 0;
	/* Initialize default scheduler */
	this->scheduler = new DefaultScheduler();

	this->current_action = NULL;
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

void ModelChecker::check_current_action(void)
{
	if (this->current_action)
		this->action_trace.push_back(this->current_action);
	else
		DEBUG("trying to push NULL action...\n");
}

ModelAction::ModelAction(action_type_t type, memory_order order, void *loc, int value)
{
	struct thread *t = thread_current();
	ModelAction *act = this;

	act->type = type;
	act->order = order;
	act->location = loc;
	act->tid = t->id;
	act->value = value;
}

void ModelAction::print(void)
{
	const char *type_str;
	switch (this->type) {
	case THREAD_CREATE:
		type_str = "thread create";
		break;
	case THREAD_YIELD:
		type_str = "thread yield";
		break;
	case THREAD_JOIN:
		type_str = "thread join";
		break;
	case ATOMIC_READ:
		type_str = "atomic read";
		break;
	case ATOMIC_WRITE:
		type_str = "atomic write";
		break;
	default:
		type_str = "unknown type";
	}

	printf("Thread: %d\tAction: %s\tMO: %d\tLoc: %#014zx\tValue: %d\n", tid, type_str, order, (size_t)location, value);
}
