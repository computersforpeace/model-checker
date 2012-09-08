/** @file main.cc
 *  @brief Entry point for the model checker.
 */

#include <unistd.h>

#include "libthreads.h"
#include "common.h"
#include "threads.h"

#include "datarace.h"

/* global "model" object */
#include "model.h"
#include "snapshot-interface.h"

static void print_usage() {
	printf(
"Usage: <program name> [OPTIONS]\n"
"\n"
"Options:\n"
"-h                    Display this help message and exit\n"
);
	exit(EXIT_SUCCESS);
}

static void parse_options(struct model_params *params, int argc, char **argv) {
	const char *shortopts = "h";
	int opt;
	bool error = false;
	while (!error && (opt = getopt(argc, argv, shortopts)) != -1) {
		switch (opt) {
		case 'h':
			print_usage();
			break;
		default: /* '?' */
			error = true;
			break;
		}
	}
	if (error)
		print_usage();
}

int main_argc;
char **main_argv;

/** The real_main function contains the main model checking loop. */
static void real_main() {
	thrd_t user_thread;
	struct model_params params;

	parse_options(&params, main_argc, main_argv);

	//Initialize race detector
	initRaceDetector();

	//Create the singleton SnapshotStack object
	snapshotObject = new SnapshotStack();

	model = new ModelChecker(params);

	snapshotObject->snapshotStep(0);
	do {
		/* Start user program */
		model->add_thread(new Thread(&user_thread, (void (*)(void *)) &user_main, NULL));

		/* Wait for all threads to complete */
		model->finish_execution();
	} while (model->next_execution());

	delete model;

	DEBUG("Exiting\n");
}

/**
 * Main function.  Just initializes snapshotting library and the
 * snapshotting library calls the real_main function.
 */
int main(int argc, char ** argv) {
	main_argc = argc;
	main_argv = argv;

	/* Let's jump in quickly and start running stuff */
	initSnapShotLibrary(10000, 1024, 1024, 1000, &real_main);
}
