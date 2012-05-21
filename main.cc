/* -*- Mode: C; indent-tabs-mode: t -*- */

#include "libthreads.h"
#include "common.h"
#include "threads.h"

/* global "model" object */
#include "model.h"
#include "snapshot.h"
#include "snapshot-interface.h"

/*
 * Return 1 if found next thread, 0 otherwise
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

static void thread_wait_finish(void) {
	DBG();

	while (!thread_system_next());
}

void real_main() {
	thrd_t user_thread;
	ucontext_t main_context;

	//Create the singleton snapshotStack object
	snapshotObject = new snapshotStack();
  
	model = new ModelChecker();
  
	if (getcontext(&main_context))
		return;
  
	model->set_system_context(&main_context);

	do {
		/* Start user program */
		model->add_thread(new Thread(&user_thread, &user_main, NULL));
    
		/* Wait for all threads to complete */
		thread_wait_finish();
	} while (model->next_execution());
  
	delete model;
  
	DEBUG("Exiting\n");
}

int main_numargs;
char ** main_args;

/*
 * Main system function
 */
int main(int numargs, char ** args) {
	/* Stash this stuff in case someone wants it eventually */
	main_numargs=numargs;
	main_args=args;

	/* Let's jump in quickly and start running stuff */
	initSnapShotLibrary(10000 /*int numbackingpages*/, 1024 /*unsigned int numsnapshots*/, 1024 /*unsigned int nummemoryregions*/ , 1000 /*int numheappages*/, &real_main /*MyFuncPtr entryPoint*/);
}
