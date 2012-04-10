#include "libatomic.h"
#include "threads_internal.h"

void atomic_store_explicit(struct atomic_object *obj, int value, memory_order order)
{
	thread_switch_to_master();
}

int atomic_load_explicit(struct atomic_object *obj, memory_order order)
{
	thread_switch_to_master();
	return 0;
}
