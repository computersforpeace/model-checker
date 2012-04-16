#include "libatomic.h"
#include "model.h"
#include "threads_internal.h"

void atomic_store_explicit(struct atomic_object *obj, int value, memory_order order)
{
	thread_current()->switch_to_master(new ModelAction(ATOMIC_WRITE, order, obj, value));
}

int atomic_load_explicit(struct atomic_object *obj, memory_order order)
{
	thread_current()->switch_to_master(new ModelAction(ATOMIC_READ, order, obj, VALUE_NONE));
	return 0;
}
