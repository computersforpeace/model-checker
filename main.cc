/** @file main.cc
 *  @brief Entry point for the model checker.
 */

#include <unistd.h>

#include "common.h"
#include "output.h"

#include "datarace.h"

/* global "model" object */
#include "model.h"
#include "snapshot-interface.h"

static void param_defaults(struct model_params *params)
{
	params->maxreads = 0;
	params->maxfuturedelay = 100;
	params->fairwindow = 0;
	params->yieldon = false;
	params->enabledcount = 1;
	params->bound = 0;
	params->maxfuturevalues = 0;
	params->expireslop = 10;
	params->verbose = !!DBG_ENABLED();
	params->uninitvalue = 0;
}

static void print_usage(struct model_params *params)
{
	/* Reset defaults before printing */
	param_defaults(params);

	model_print(
"Copyright (c) 2013 Regents of the University of California. All rights reserved.\n"
"Distributed under the GPLv2\n"
"Usage: <program name> [MC_OPTIONS] -- [PROGRAM ARGUMENTS]\n"
"\n"
"Options:\n"
"-h                    Display this help message and exit\n"
"-m                    Maximum times a thread can read from the same write\n"
"                      while other writes exist. Default: %d\n"
"-M                    Maximum number of future values that can be sent to\n"
"                      the same read. Default: %d\n"
"-s                    Maximum actions that the model checker will wait for\n"
"                      a write from the future past the expected number of\n"
"                      actions. Default: %d\n"
"-S                    Future value expiration sloppiness. Default: %u\n"
"-f                    Specify a fairness window in which actions that are\n"
"                      enabled sufficiently many times should receive\n"
"                      priority for execution. Default: %d\n"
"-y                    Turn on CHESS yield-based fairness support.\n"
"                      Default: %d\n"
"-e                    Enabled count. Default: %d\n"
"-b                    Upper length bound. Default: %d\n"
"-v                    Print verbose execution information.\n"
"-u                    Value for uninitialized reads. Default: %u\n"
"--                    Program arguments follow.\n\n",
params->maxreads, params->maxfuturevalues, params->maxfuturedelay, params->expireslop, params->fairwindow, params->yieldon, params->enabledcount, params->bound, params->uninitvalue);
	exit(EXIT_SUCCESS);
}

static void parse_options(struct model_params *params, int argc, char **argv)
{
	const char *shortopts = "hym:M:s:S:f:e:b:u:v";
	int opt;
	bool error = false;
	while (!error && (opt = getopt(argc, argv, shortopts)) != -1) {
		switch (opt) {
		case 'h':
			print_usage(params);
			break;
		case 's':
			params->maxfuturedelay = atoi(optarg);
			break;
		case 'S':
			params->expireslop = atoi(optarg);
			break;
		case 'f':
			params->fairwindow = atoi(optarg);
			break;
		case 'e':
			params->enabledcount = atoi(optarg);
			break;
		case 'b':
			params->bound = atoi(optarg);
			break;
		case 'm':
			params->maxreads = atoi(optarg);
			break;
		case 'M':
			params->maxfuturevalues = atoi(optarg);
			break;
		case 'v':
			params->verbose = 1;
			break;
		case 'u':
			params->uninitvalue = atoi(optarg);
			break;
		case 'y':
			params->yieldon = true;
			break;
		default: /* '?' */
			error = true;
			break;
		}
	}

	/* Pass remaining arguments to user program */
	params->argc = argc - (optind - 1);
	params->argv = argv + (optind - 1);

	/* Reset program name */
	params->argv[0] = argv[0];

	/* Reset (global) optind for potential use by user program */
	optind = 1;

	if (error)
		print_usage(params);
}

int main_argc;
char **main_argv;

/** The model_main function contains the main model checking loop. */
static void model_main()
{
	struct model_params params;

	param_defaults(&params);

	parse_options(&params, main_argc, main_argv);

	//Initialize race detector
	initRaceDetector();

	snapshot_stack_init();

	model = new ModelChecker(params);
	snapshot_record(0);
	model->run();
	delete model;

	DEBUG("Exiting\n");
}

/**
 * Main function.  Just initializes snapshotting library and the
 * snapshotting library calls the model_main function.
 */
int main(int argc, char **argv)
{
	main_argc = argc;
	main_argv = argv;

	/* Configure output redirection for the model-checker */
	redirect_output();

	/* Let's jump in quickly and start running stuff */
	snapshot_system_init(10000, 1024, 1024, 4000, &model_main);
}
