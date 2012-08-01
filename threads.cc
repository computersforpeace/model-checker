#include "libthreads.h"
#include "common.h"
#include "threads.h"

/* global "model" object */
#include "model.h"

#define STACK_SIZE (1024 * 1024)

static void * stack_allocate(size_t size)
{
	return malloc(size);
}

static void stack_free(void *stack)
{
	free(stack);
}

Thread * thread_current(void)
{
	ASSERT(model);
	return model->scheduler->get_current_thread();
}

/**
 * Provides a startup wrapper for each thread, allowing some initial
 * model-checking data to be recorded. This method also gets around makecontext
 * not being 64-bit clean
 */
void thread_startup() {
	Thread * curr_thread = thread_current();

	/* TODO -- we should make this event always immediately follow the
		 CREATE event, so we don't get redundant traces...  */

	/* Add dummy "start" action, just to create a first clock vector */
	model->switch_to_master(new ModelAction(THREAD_START, memory_order_seq_cst, curr_thread));

	/* Call the actual thread function */
	curr_thread->start_routine(curr_thread->arg);
}

int Thread::create_context()
{
	int ret;

	ret = getcontext(&context);
	if (ret)
		return ret;

	/* Initialize new managed context */
	stack = stack_allocate(STACK_SIZE);
	context.uc_stack.ss_sp = stack;
	context.uc_stack.ss_size = STACK_SIZE;
	context.uc_stack.ss_flags = 0;
	context.uc_link = model->get_system_context();
	makecontext(&context, thread_startup, 0);

	return 0;
}

int Thread::swap(Thread *t, ucontext_t *ctxt)
{
	return swapcontext(&t->context, ctxt);
}

int Thread::swap(ucontext_t *ctxt, Thread *t)
{
	return swapcontext(ctxt, &t->context);
}

void Thread::complete()
{
	if (state != THREAD_COMPLETED) {
		DEBUG("completed thread %d\n", get_id());
		state = THREAD_COMPLETED;
		if (stack)
			stack_free(stack);
	}
}

Thread::Thread(thrd_t *t, void (*func)(void *), void *a) :
	start_routine(func),
	arg(a),
	user_thread(t),
	state(THREAD_CREATED),
	last_action_val(VALUE_NONE)
{
	int ret;

	/* Initialize state */
	ret = create_context();
	if (ret)
		printf("Error in create_context\n");

	id = model->get_next_id();
	*user_thread = id;
	parent = thread_current();
}

Thread::~Thread()
{
	complete();
	model->remove_thread(this);
}

thread_id_t Thread::get_id()
{
	return id;
}
