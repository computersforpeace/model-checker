/** @file main.cc
 *  @brief Entry point for the model checker.
 */

#include "libthreads.h"
#include "common.h"
#include "threads.h"

#include "datarace.h"

/* global "model" object */
#include "model.h"
#include "snapshot-interface.h"

/** The thread_system_next function takes the next step in the
 *  execution.  @return Returns the 1 if there is another step and 0
 *  otherwise.
 */

static int thread_system_next(void) {
	Thread *curr, *next;

	curr = thread_current();
	if (curr) {
		if (curr->get_state() == THREAD_READY) {
			model->check_current_action();
			model->scheduler->add_thread(curr);
		} else if (curr->get_state() == THREAD_RUNNING)
			/* Stopped while running; i.e., completed */
			curr->complete();
		else
			ASSERT(false);
	}
	next = model->scheduler->next_thread();
	if (next)
		next->set_state(THREAD_RUNNING);
	DEBUG("(%d, %d)\n", curr ? curr->get_id() : -1, next ? next->get_id() : -1);
	if (!next)
		return 1;
	return Thread::swap(model->get_system_context(), next);
}

/** The thread_wait_finish method runs the current execution until we
 *  have no more steps to take.
 */

static void thread_wait_finish(void) {
	DBG();

	while (!thread_system_next());
}


/** The real_main function contains the main model checking loop. */

static void real_main() {
	thrd_t user_thread;
	ucontext_t main_context;

	//Initialize race detector
	initRaceDetector();

	//Create the singleton SnapshotStack object
	snapshotObject = new SnapshotStack();

	model = new ModelChecker();

	if (getcontext(&main_context))
		return;

	model->set_system_context(&main_context);

	snapshotObject->snapshotStep(0);

	do {
		/* Start user program */
		model->add_thread(new Thread(&user_thread, (void (*)(void *)) &user_main, NULL));

		/* Wait for all threads to complete */
		thread_wait_finish();
	} while (model->next_execution());

	delete model;

	DEBUG("Exiting\n");
}

int main_numargs;
char ** main_args;

/**
 * Main function.  Just initializes snapshotting library and the
 * snapshotting library calls the real_main function.
 */
int main(int numargs, char ** args) {
	/* Stash this stuff in case someone wants it eventually */
	main_numargs=numargs;
	main_args=args;

	/* Let's jump in quickly and start running stuff */
	initSnapShotLibrary(10000, 1024, 1024, 1000, &real_main);
}
