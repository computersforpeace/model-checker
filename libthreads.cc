#include <threads.h>
#include "common.h"
#include "threads-model.h"
#include "action.h"

/* global "model" object */
#include "model.h"

/*
 * User program API functions
 */
int thrd_create(thrd_t *t, thrd_start_t start_routine, void *arg)
{
	struct thread_params params = { start_routine, arg };
	/* seq_cst is just a 'don't care' parameter */
	model->switch_to_master(new ModelAction(THREAD_CREATE, std::memory_order_seq_cst, t, (uint64_t)&params));
	return 0;
}

int thrd_join(thrd_t t)
{
	Thread *th = t.priv;
	model->switch_to_master(new ModelAction(THREAD_JOIN, std::memory_order_seq_cst, th, id_to_int(thrd_to_id(t))));
	return 0;
}

/** A no-op, for now */
void thrd_yield(void)
{
	model->switch_to_master(new ModelAction(THREAD_YIELD, std::memory_order_seq_cst, thread_current(), VALUE_NONE));
}

thrd_t thrd_current(void)
{
	return thread_current()->get_thrd_t();
}
