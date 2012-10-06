/** @file threads.cc
 *  @brief Thread functions.
 */

#include "libthreads.h"
#include "common.h"
#include "threads.h"

/* global "model" object */
#include "model.h"

/** Allocate a stack for a new thread. */
static void * stack_allocate(size_t size)
{
	return snapshot_malloc(size);
}

/** Free a stack for a terminated thread. */
static void stack_free(void *stack)
{
	snapshot_free(stack);
}

/** Return the currently executing thread. */
Thread * thread_current(void)
{
	ASSERT(model);
	return model->get_current_thread();
}

/**
 * Provides a startup wrapper for each thread, allowing some initial
 * model-checking data to be recorded. This method also gets around makecontext
 * not being 64-bit clean
 * @todo We should make the START event always immediately follow the
 * CREATE event, so we don't get redundant traces...
 */
void thread_startup()
{
	Thread * curr_thread = thread_current();

	/* Add dummy "start" action, just to create a first clock vector */
	model->switch_to_master(new ModelAction(THREAD_START, std::memory_order_seq_cst, curr_thread));

	/* Call the actual thread function */
	curr_thread->start_routine(curr_thread->arg);

	/* Finish thread properly */
	model->switch_to_master(new ModelAction(THREAD_FINISH, std::memory_order_seq_cst, curr_thread));
}

/**
 * Create a thread context for a new thread so we can use
 * setcontext/getcontext/swapcontext to swap it out.
 * @return 0 on success; otherwise, non-zero error condition
 */
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

/**
 * Swaps the current context to another thread of execution. This form switches
 * from a user Thread to a system context.
 * @param t Thread representing the currently-running thread. The current
 * context is saved here.
 * @param ctxt Context to which we will swap. Must hold a valid system context.
 * @return Does not return, unless we return to Thread t's context. See
 * swapcontext(3) (returns 0 for success, -1 for failure).
 */
int Thread::swap(Thread *t, ucontext_t *ctxt)
{
	return swapcontext(&t->context, ctxt);
}

/**
 * Swaps the current context to another thread of execution. This form switches
 * from a system context to a user Thread.
 * @param ctxt System context variable to which to save the current context.
 * @param t Thread to which we will swap. Must hold a valid user context.
 * @return Does not return, unless we return to the system context (ctxt). See
 * swapcontext(3) (returns 0 for success, -1 for failure).
 */
int Thread::swap(ucontext_t *ctxt, Thread *t)
{
	return swapcontext(ctxt, &t->context);
}


/** Terminate a thread and free its stack. */
void Thread::complete()
{
	if (!is_complete()) {
		DEBUG("completed thread %d\n", id_to_int(get_id()));
		state = THREAD_COMPLETED;
		if (stack)
			stack_free(stack);
	}
}

/**
 * Construct a new thread.
 * @param t The thread identifier of the newly created thread.
 * @param func The function that the thread will call.
 * @param a The parameter to pass to this function.
 */
Thread::Thread(thrd_t *t, void (*func)(void *), void *a) :
	creation(NULL),
	pending(NULL),
	start_routine(func),
	arg(a),
	user_thread(t),
	state(THREAD_CREATED),
	wait_list(),
	last_action_val(VALUE_NONE),
	model_thread(false)
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

/** Destructor */
Thread::~Thread()
{
	complete();
	model->remove_thread(this);
}

/** @return The thread_id_t corresponding to this Thread object. */
thread_id_t Thread::get_id()
{
	return id;
}
