#include "libthreads.h"
#include "common.h"
#include "threads.h"

/* global "model" object */
#include "model.h"

/*
 * User program API functions
 */
int thrd_create(thrd_t *t, thrd_start_t start_routine, void *arg)
{
	Thread *thread;
	int ret;
	DBG();
	thread = new Thread(t, start_routine, arg);
	ret = model->add_thread(thread);
	DEBUG("create thread %d\n", id_to_int(thrd_to_id(*t)));
	/* seq_cst is just a 'don't care' parameter */
	model->switch_to_master(new ModelAction(THREAD_CREATE, memory_order_seq_cst, thread, VALUE_NONE));
	return ret;
}

int thrd_join(thrd_t t)
{
	Thread *th = model->get_thread(thrd_to_id(t));
	while (th->get_state() != THREAD_COMPLETED)
		model->switch_to_master(NULL);
	return 0;
}

int thrd_yield(void)
{
	/* seq_cst is just a 'don't care' parameter */
	return model->switch_to_master(new ModelAction(THREAD_YIELD, memory_order_seq_cst, NULL, VALUE_NONE));
}

thrd_t thrd_current(void)
{
	return thread_current()->get_thrd_t();
}
