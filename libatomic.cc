#include "libatomic.h"
#include "libthreads.h"

void atomic_store_explicit(struct atomic_object *obj, int value, memory_order order)
{
	thread_yield();
}

int atomic_load_explicit(struct atomic_object *obj, memory_order order)
{
	thread_yield();
	return 0;
}
