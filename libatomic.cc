#include "libatomic.h"
#include "model.h"
#include "common.h"

void atomic_store_explicit(struct atomic_object *obj, int value, memory_order order)
{
	DBG();
	obj->value = value;
	model->switch_to_master(new ModelAction(ATOMIC_WRITE, order, obj, value));
}

int atomic_load_explicit(struct atomic_object *obj, memory_order order)
{
	DBG();
	model->switch_to_master(new ModelAction(ATOMIC_READ, order, obj));
	return (int) thread_current()->get_return_value();
}

void atomic_init(struct atomic_object *obj, int value)
{
	DBG();
	obj->value = value;
	model->switch_to_master(new ModelAction(ATOMIC_INIT, memory_order_relaxed, obj, value));
}
