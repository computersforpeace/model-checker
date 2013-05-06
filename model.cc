#include <stdio.h>
#include <algorithm>
#include <new>
#include <stdarg.h>

#include "model.h"
#include "action.h"
#include "nodestack.h"
#include "schedule.h"
#include "snapshot-interface.h"
#include "common.h"
#include "datarace.h"
#include "threads-model.h"
#include "output.h"
#include "traceanalysis.h"
#include "execution.h"
#include "bugmessage.h"

ModelChecker *model;

/** @brief Constructor */
ModelChecker::ModelChecker(struct model_params params) :
	/* Initialize default scheduler */
	params(params),
	scheduler(new Scheduler()),
	node_stack(new NodeStack()),
	execution(new ModelExecution(this, &this->params, scheduler, node_stack)),
	execution_number(1),
	diverge(NULL),
	earliest_diverge(NULL),
	trace_analyses()
{
}

/** @brief Destructor */
ModelChecker::~ModelChecker()
{
	delete node_stack;
	delete scheduler;
}

/**
 * Restores user program to initial state and resets all model-checker data
 * structures.
 */
void ModelChecker::reset_to_initial_state()
{
	DEBUG("+++ Resetting to initial state +++\n");
	node_stack->reset_execution();

	/**
	 * FIXME: if we utilize partial rollback, we will need to free only
	 * those pending actions which were NOT pending before the rollback
	 * point
	 */
	for (unsigned int i = 0; i < get_num_threads(); i++)
		delete get_thread(int_to_id(i))->get_pending();

	snapshot_backtrack_before(0);
}

/** @return the number of user threads created during this execution */
unsigned int ModelChecker::get_num_threads() const
{
	return execution->get_num_threads();
}

/**
 * Must be called from user-thread context (e.g., through the global
 * thread_current() interface)
 *
 * @return The currently executing Thread.
 */
Thread * ModelChecker::get_current_thread() const
{
	return scheduler->get_current_thread();
}

/**
 * @brief Choose the next thread to execute.
 *
 * This function chooses the next thread that should execute. It can enforce
 * execution replay/backtracking or, if the model-checker has no preference
 * regarding the next thread (i.e., when exploring a new execution ordering),
 * we defer to the scheduler.
 *
 * @return The next chosen thread to run, if any exist. Or else if the current
 * execution should terminate, return NULL.
 */
Thread * ModelChecker::get_next_thread()
{
	thread_id_t tid;

	/*
	 * Have we completed exploring the preselected path? Then let the
	 * scheduler decide
	 */
	if (diverge == NULL)
		return scheduler->select_next_thread(node_stack->get_head());


	/* Else, we are trying to replay an execution */
	ModelAction *next = node_stack->get_next()->get_action();

	if (next == diverge) {
		if (earliest_diverge == NULL || *diverge < *earliest_diverge)
			earliest_diverge = diverge;

		Node *nextnode = next->get_node();
		Node *prevnode = nextnode->get_parent();
		scheduler->update_sleep_set(prevnode);

		/* Reached divergence point */
		if (nextnode->increment_behaviors()) {
			/* Execute the same thread with a new behavior */
			tid = next->get_tid();
			node_stack->pop_restofstack(2);
		} else {
			ASSERT(prevnode);
			/* Make a different thread execute for next step */
			scheduler->add_sleep(get_thread(next->get_tid()));
			tid = prevnode->get_next_backtrack();
			/* Make sure the backtracked thread isn't sleeping. */
			node_stack->pop_restofstack(1);
			if (diverge == earliest_diverge) {
				earliest_diverge = prevnode->get_action();
			}
		}
		/* Start the round robin scheduler from this thread id */
		scheduler->set_scheduler_thread(tid);
		/* The correct sleep set is in the parent node. */
		execute_sleep_set();

		DEBUG("*** Divergence point ***\n");

		diverge = NULL;
	} else {
		tid = next->get_tid();
	}
	DEBUG("*** ModelChecker chose next thread = %d ***\n", id_to_int(tid));
	ASSERT(tid != THREAD_ID_T_NONE);
	return get_thread(id_to_int(tid));
}

/**
 * We need to know what the next actions of all threads in the sleep
 * set will be.  This method computes them and stores the actions at
 * the corresponding thread object's pending action.
 */
void ModelChecker::execute_sleep_set()
{
	for (unsigned int i = 0; i < get_num_threads(); i++) {
		thread_id_t tid = int_to_id(i);
		Thread *thr = get_thread(tid);
		if (scheduler->is_sleep_set(thr) && thr->get_pending()) {
			thr->get_pending()->set_sleep_flag();
		}
	}
}

/**
 * @brief Assert a bug in the executing program.
 *
 * Use this function to assert any sort of bug in the user program. If the
 * current trace is feasible (actually, a prefix of some feasible execution),
 * then this execution will be aborted, printing the appropriate message. If
 * the current trace is not yet feasible, the error message will be stashed and
 * printed if the execution ever becomes feasible.
 *
 * @param msg Descriptive message for the bug (do not include newline char)
 * @return True if bug is immediately-feasible
 */
bool ModelChecker::assert_bug(const char *msg, ...)
{
	char str[800];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(str, sizeof(str), msg, ap);
	va_end(ap);

	return execution->assert_bug(str);
}

/**
 * @brief Assert a bug in the executing program, asserted by a user thread
 * @see ModelChecker::assert_bug
 * @param msg Descriptive message for the bug (do not include newline char)
 */
void ModelChecker::assert_user_bug(const char *msg)
{
	/* If feasible bug, bail out now */
	if (assert_bug(msg))
		switch_to_master(NULL);
}

/** @brief Print bug report listing for this execution (if any bugs exist) */
void ModelChecker::print_bugs() const
{
	if (execution->have_bug_reports()) {
		SnapVector<bug_message *> *bugs = execution->get_bugs();

		model_print("Bug report: %zu bug%s detected\n",
				bugs->size(),
				bugs->size() > 1 ? "s" : "");
		for (unsigned int i = 0; i < bugs->size(); i++)
			(*bugs)[i]->print();
	}
}

/**
 * @brief Record end-of-execution stats
 *
 * Must be run when exiting an execution. Records various stats.
 * @see struct execution_stats
 */
void ModelChecker::record_stats()
{
	stats.num_total++;
	if (!execution->isfeasibleprefix())
		stats.num_infeasible++;
	else if (execution->have_bug_reports())
		stats.num_buggy_executions++;
	else if (execution->is_complete_execution())
		stats.num_complete++;
	else {
		stats.num_redundant++;

		/**
		 * @todo We can violate this ASSERT() when fairness/sleep sets
		 * conflict to cause an execution to terminate, e.g. with:
		 * Scheduler: [0: disabled][1: disabled][2: sleep][3: current, enabled]
		 */
		//ASSERT(scheduler->all_threads_sleeping());
	}
}

/** @brief Print execution stats */
void ModelChecker::print_stats() const
{
	model_print("Number of complete, bug-free executions: %d\n", stats.num_complete);
	model_print("Number of redundant executions: %d\n", stats.num_redundant);
	model_print("Number of buggy executions: %d\n", stats.num_buggy_executions);
	model_print("Number of infeasible executions: %d\n", stats.num_infeasible);
	model_print("Total executions: %d\n", stats.num_total);
	model_print("Total nodes created: %d\n", node_stack->get_total_nodes());
}

/**
 * @brief End-of-exeuction print
 * @param printbugs Should any existing bugs be printed?
 */
void ModelChecker::print_execution(bool printbugs) const
{
	print_program_output();

	if (params.verbose) {
		model_print("Earliest divergence point since last feasible execution:\n");
		if (earliest_diverge)
			earliest_diverge->print();
		else
			model_print("(Not set)\n");

		model_print("\n");
		print_stats();
	}

	/* Don't print invalid bugs */
	if (printbugs)
		print_bugs();

	model_print("\n");
	execution->print_summary();
}

/**
 * Queries the model-checker for more executions to explore and, if one
 * exists, resets the model-checker state to execute a new execution.
 *
 * @return If there are more executions to explore, return true. Otherwise,
 * return false.
 */
bool ModelChecker::next_execution()
{
	DBG();
	/* Is this execution a feasible execution that's worth bug-checking? */
	bool complete = execution->isfeasibleprefix() &&
		(execution->is_complete_execution() ||
		 execution->have_bug_reports());

	/* End-of-execution bug checks */
	if (complete) {
		if (execution->is_deadlocked())
			assert_bug("Deadlock detected");

		checkDataRaces();
		run_trace_analyses();
	}

	record_stats();

	/* Output */
	if (params.verbose || (complete && execution->have_bug_reports()))
		print_execution(complete);
	else
		clear_program_output();

	if (complete)
		earliest_diverge = NULL;

	if ((diverge = execution->get_next_backtrack()) == NULL)
		return false;

	if (DBG_ENABLED()) {
		model_print("Next execution will diverge at:\n");
		diverge->print();
	}

	execution_number++;

	reset_to_initial_state();
	return true;
}

/** @brief Run trace analyses on complete trace */
void ModelChecker::run_trace_analyses() {
	for (unsigned int i = 0; i < trace_analyses.size(); i++)
		trace_analyses[i]->analyze(execution->get_action_trace());
}

/**
 * @brief Get a Thread reference by its ID
 * @param tid The Thread's ID
 * @return A Thread reference
 */
Thread * ModelChecker::get_thread(thread_id_t tid) const
{
	return execution->get_thread(tid);
}

/**
 * @brief Get a reference to the Thread in which a ModelAction was executed
 * @param act The ModelAction
 * @return A Thread reference
 */
Thread * ModelChecker::get_thread(const ModelAction *act) const
{
	return execution->get_thread(act);
}

/**
 * @brief Check if a Thread is currently enabled
 * @param t The Thread to check
 * @return True if the Thread is currently enabled
 */
bool ModelChecker::is_enabled(Thread *t) const
{
	return scheduler->is_enabled(t);
}

/**
 * @brief Check if a Thread is currently enabled
 * @param tid The ID of the Thread to check
 * @return True if the Thread is currently enabled
 */
bool ModelChecker::is_enabled(thread_id_t tid) const
{
	return scheduler->is_enabled(tid);
}

/**
 * Switch from a model-checker context to a user-thread context. This is the
 * complement of ModelChecker::switch_to_master and must be called from the
 * model-checker context
 *
 * @param thread The user-thread to switch to
 */
void ModelChecker::switch_from_master(Thread *thread)
{
	scheduler->set_current_thread(thread);
	Thread::swap(&system_context, thread);
}

/**
 * Switch from a user-context to the "master thread" context (a.k.a. system
 * context). This switch is made with the intention of exploring a particular
 * model-checking action (described by a ModelAction object). Must be called
 * from a user-thread context.
 *
 * @param act The current action that will be explored. May be NULL only if
 * trace is exiting via an assertion (see ModelExecution::set_assert and
 * ModelExecution::has_asserted).
 * @return Return the value returned by the current action
 */
uint64_t ModelChecker::switch_to_master(ModelAction *act)
{
	DBG();
	Thread *old = thread_current();
	scheduler->set_current_thread(NULL);
	ASSERT(!old->get_pending());
	old->set_pending(act);
	if (Thread::swap(old, &system_context) < 0) {
		perror("swap threads");
		exit(EXIT_FAILURE);
	}
	return old->get_return_value();
}

/** Wrapper to run the user's main function, with appropriate arguments */
void user_main_wrapper(void *)
{
	user_main(model->params.argc, model->params.argv);
}

bool ModelChecker::should_terminate_execution()
{
	/* Infeasible -> don't take any more steps */
	if (execution->is_infeasible())
		return true;
	else if (execution->isfeasibleprefix() && execution->have_bug_reports()) {
		execution->set_assert();
		return true;
	}

	if (execution->too_many_steps())
		return true;
	return false;
}

/** @brief Run ModelChecker for the user program */
void ModelChecker::run()
{
	do {
		thrd_t user_thread;
		Thread *t = new Thread(execution->get_next_id(), &user_thread, &user_main_wrapper, NULL, NULL);
		execution->add_thread(t);

		do {
			/*
			 * Stash next pending action(s) for thread(s). There
			 * should only need to stash one thread's action--the
			 * thread which just took a step--plus the first step
			 * for any newly-created thread
			 */
			for (unsigned int i = 0; i < get_num_threads(); i++) {
				thread_id_t tid = int_to_id(i);
				Thread *thr = get_thread(tid);
				if (!thr->is_model_thread() && !thr->is_complete() && !thr->get_pending()) {
					switch_from_master(thr);
					if (thr->is_waiting_on(thr))
						assert_bug("Deadlock detected (thread %u)", i);
				}
			}

			/* Don't schedule threads which should be disabled */
			for (unsigned int i = 0; i < get_num_threads(); i++) {
				Thread *th = get_thread(int_to_id(i));
				ModelAction *act = th->get_pending();
				if (act && is_enabled(th) && !execution->check_action_enabled(act)) {
					scheduler->sleep(th);
				}
			}

			/* Catch assertions from prior take_step or from
			 * between-ModelAction bugs (e.g., data races) */
			if (execution->has_asserted())
				break;

			if (!t)
				t = get_next_thread();
			if (!t || t->is_model_thread())
				break;

			/* Consume the next action for a Thread */
			ModelAction *curr = t->get_pending();
			t->set_pending(NULL);
			t = execution->take_step(curr);
		} while (!should_terminate_execution());

	} while (next_execution());

	execution->fixup_release_sequences();

	model_print("******* Model-checking complete: *******\n");
	print_stats();

	/* Have the trace analyses dump their output. */
	for (unsigned int i = 0; i < trace_analyses.size(); i++)
		trace_analyses[i]->finish();
}
