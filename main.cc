/** @file main.cc
 *  @brief Entry point for the model checker.
 */

#include <unistd.h>
#include <getopt.h>

#include "common.h"
#include "output.h"

#include "datarace.h"

/* global "model" object */
#include "model.h"
#include "params.h"
#include "snapshot-interface.h"
#include "scanalysis.h"

static void param_defaults(struct model_params *params)
{
	params->maxreads = 0;
	params->maxfuturedelay = 6;
	params->fairwindow = 0;
	params->yieldon = false;
	params->yieldblock = false;
	params->sc_trace_analysis = false;
	params->enabledcount = 1;
	params->bound = 0;
	params->maxfuturevalues = 0;
	params->expireslop = 4;
	params->verbose = !!DBG_ENABLED();
	params->uninitvalue = 0;
}

static void print_usage(const char *program_name, struct model_params *params)
{
	/* Reset defaults before printing */
	param_defaults(params);

	model_print(
"Copyright (c) 2013 Regents of the University of California. All rights reserved.\n"
"Distributed under the GPLv2\n"
"Written by Brian Norris and Brian Demsky\n"
"\n"
"Usage: %s [MODEL-CHECKER OPTIONS] -- [PROGRAM ARGS]\n"
"\n"
"MODLE-CHECKER OPTIONS can be any of the model-checker options listed below. Arguments\n"
"provided after the `--' (the PROGRAM ARGS) are passed to the user program.\n"
"\n"
"Model-checker options:\n"
"-h, --help                  Display this help message and exit\n"
"-m, --liveness=NUM          Maximum times a thread can read from the same write\n"
"                              while other writes exist.\n"
"                              Default: %d\n"
"-M, --maxfv=NUM             Maximum number of future values that can be sent to\n"
"                              the same read.\n"
"                              Default: %d\n"
"-s, --maxfvdelay=NUM        Maximum actions that the model checker will wait for\n"
"                              a write from the future past the expected number\n"
"                              of actions.\n"
"                              Default: %d\n"
"-S, --fvslop=NUM            Future value expiration sloppiness.\n"
"                              Default: %u\n"
"-y, --yield                 Enable CHESS-like yield-based fairness support.\n"
"                              Default: %s\n"
"-Y, --yieldblock            Prohibit an execution from running a yield.\n"
"                              Default: %s\n"
"-f, --fairness=WINDOW       Specify a fairness window in which actions that are\n"
"                              enabled sufficiently many times should receive\n"
"                              priority for execution (not recommended).\n"
"                              Default: %d\n"
"-e, --enabled=COUNT         Enabled count.\n"
"                              Default: %d\n"
"-b, --bound=MAX             Upper length bound.\n"
"                              Default: %d\n"
"-v, --verbose               Print verbose execution information.\n"
"-u, --uninitialized=VALUE   Return VALUE any load which may read from an\n"
"                              uninitialized atomic.\n"
"                              Default: %u\n"
"-c, --analysis              Use SC Trace Analysis.\n"
" --                         Program arguments follow.\n\n",
		program_name,
		params->maxreads,
		params->maxfuturevalues,
		params->maxfuturedelay,
		params->expireslop,
		params->yieldon ? "enabled" : "disabled",
		params->yieldblock ? "enabled" : "disabled",
		params->fairwindow,
		params->enabledcount,
		params->bound,
		params->uninitvalue);
	exit(EXIT_SUCCESS);
}

static void parse_options(struct model_params *params, int argc, char **argv)
{
	const char *shortopts = "hyYcm:M:s:S:f:e:b:u:v::";
	const struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"liveness", required_argument, NULL, 'm'},
		{"maxfv", required_argument, NULL, 'M'},
		{"maxfvdelay", required_argument, NULL, 's'},
		{"fvslop", required_argument, NULL, 'S'},
		{"fairness", required_argument, NULL, 'f'},
		{"yield", no_argument, NULL, 'y'},
		{"yieldblock", no_argument, NULL, 'Y'},
		{"enabled", required_argument, NULL, 'e'},
		{"bound", required_argument, NULL, 'b'},
		{"verbose", optional_argument, NULL, 'v'},
		{"uninitialized", optional_argument, NULL, 'u'},
		{"analysis", optional_argument, NULL, 'c'},
		{0, 0, 0, 0} /* Terminator */
	};
	int opt, longindex;
	bool error = false;
	while (!error && (opt = getopt_long(argc, argv, shortopts, longopts, &longindex)) != -1) {
		switch (opt) {
		case 'h':
			print_usage(argv[0], params);
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
			params->verbose = optarg ? atoi(optarg) : 1;
			break;
		case 'u':
			params->uninitvalue = atoi(optarg);
			break;
		case 'c':
			params->sc_trace_analysis = true;
			break;
		case 'y':
			params->yieldon = true;
			break;
		case 'Y':
			params->yieldblock = true;
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
		print_usage(argv[0], params);
}

int main_argc;
char **main_argv;

static void install_trace_analyses(const ModelExecution *execution)
{
	if (model->params.sc_trace_analysis)
		model->add_trace_analysis(new SCAnalysis(execution));
}

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
	install_trace_analyses(model->get_execution());

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
