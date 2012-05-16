#include <stdio.h>

#include "model.h"
#include "action.h"
#include "clockvector.h"

ModelAction::ModelAction(action_type_t type, memory_order order, void *loc, int value)
{
	Thread *t = thread_current();
	ModelAction *act = this;

	act->type = type;
	act->order = order;
	act->location = loc;
	act->tid = t->get_id();
	act->value = value;
	act->seq_number = model->get_next_seq_num();

	cv = NULL;
}

ModelAction::~ModelAction()
{
	if (cv)
		delete cv;
}

bool ModelAction::is_read()
{
	return type == ATOMIC_READ;
}

bool ModelAction::is_write()
{
	return type == ATOMIC_WRITE;
}

bool ModelAction::is_acquire()
{
	switch (order) {
	case memory_order_acquire:
	case memory_order_acq_rel:
	case memory_order_seq_cst:
		return true;
	default:
		return false;
	}
}

bool ModelAction::is_release()
{
	switch (order) {
	case memory_order_release:
	case memory_order_acq_rel:
	case memory_order_seq_cst:
		return true;
	default:
		return false;
	}
}

bool ModelAction::same_var(ModelAction *act)
{
	return location == act->location;
}

bool ModelAction::same_thread(ModelAction *act)
{
	return tid == act->tid;
}

bool ModelAction::is_dependent(ModelAction *act)
{
	if (!is_read() && !is_write())
		return false;
	if (!act->is_read() && !act->is_write())
		return false;
	if (same_var(act) && !same_thread(act) &&
			(is_write() || act->is_write()))
		return true;
	return false;
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

	printf("(%4d) Thread: %d\tAction: %s\tMO: %d\tLoc: %14p\tValue: %d\n",
			seq_number, id_to_int(tid), type_str, order, location, value);
}
