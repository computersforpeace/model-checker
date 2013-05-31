#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <model-assert.h>

#include "common.h"
#include "model.h"
#include "stacktrace.h"
#include "output.h"

#define MAX_TRACE_LEN 100

/** @brief Model-checker output file descriptor; default to stdout until redirected */
int model_out = STDOUT_FILENO;

#define CONFIG_STACKTRACE
/** Print a backtrace of the current program state. */
void print_trace(void)
{
#ifdef CONFIG_STACKTRACE
	print_stacktrace(model_out);
#else
	void *array[MAX_TRACE_LEN];
	char **strings;
	int size, i;

	size = backtrace(array, MAX_TRACE_LEN);
	strings = backtrace_symbols(array, size);

	model_print("\nDumping stack trace (%d frames):\n", size);

	for (i = 0; i < size; i++)
		model_print("\t%s\n", strings[i]);

	free(strings);
#endif /* CONFIG_STACKTRACE */
}

void assert_hook(void)
{
	model_print("Add breakpoint to line %u in file %s.\n", __LINE__, __FILE__);
}

void model_assert(bool expr, const char *file, int line)
{
	if (!expr) {
		char msg[100];
		sprintf(msg, "Program has hit assertion in file %s at line %d\n",
				file, line);
		model->assert_user_bug(msg);
	}
}

#ifndef CONFIG_DEBUG

static int fd_user_out; /**< @brief File descriptor from which to read user program output */

/**
 * @brief Setup output redirecting
 *
 * Redirects user program's stdout to a pipe so that we can dump it
 * selectively, when displaying bugs, etc.
 * Also connects a file descriptor 'model_out' directly to stdout, for printing
 * data when needed.
 *
 * The model-checker can selectively choose to print/hide the user program
 * output.
 * @see clear_program_output
 * @see print_program_output
 *
 * Note that the user program's pipe has limited memory, so if a program will
 * output much data, we will need to buffer it in user-space during execution.
 * This also means that if ModelChecker decides not to print an execution, it
 * should promptly clear the pipe.
 *
 * This function should only be called once.
 */
void redirect_output()
{
	/* Save stdout for later use */
	model_out = dup(STDOUT_FILENO);
	if (model_out < 0) {
		perror("dup");
		exit(EXIT_FAILURE);
	}

	/* Redirect program output to a pipe */
	int pipefd[2];
	if (pipe(pipefd) < 0) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}
	if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
		perror("dup2");
		exit(EXIT_FAILURE);
	}
	close(pipefd[1]);

	/* Save the "read" side of the pipe for use later */
	if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) < 0) {
		perror("fcntl");
		exit(EXIT_FAILURE);
	}
	fd_user_out = pipefd[0];
}

/**
 * @brief Wrapper for reading data to buffer
 *
 * Besides a simple read, this handles the subtleties of EOF and nonblocking
 * input (if fd is O_NONBLOCK).
 *
 * @param fd The file descriptor to read.
 * @param buf Buffer to read to.
 * @param maxlen Maximum data to read to buffer
 * @return The length of data read. If zero, then we hit EOF or ran out of data
 * (non-blocking)
 */
static ssize_t read_to_buf(int fd, char *buf, size_t maxlen)
{
	ssize_t ret = read(fd, buf, maxlen);
	if (ret < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0;
		} else {
			perror("read");
			exit(EXIT_FAILURE);
		}
	}
	return ret;
}

/** @brief Dump any pending program output without printing */
void clear_program_output()
{
	fflush(stdout);
	char buf[200];
	while (read_to_buf(fd_user_out, buf, sizeof(buf)));
}

/** @brief Print out any pending program output */
void print_program_output()
{
	char buf[200];

	model_print("---- BEGIN PROGRAM OUTPUT ----\n");

	/* Gather all program output */
	fflush(stdout);

	/* Read program output pipe and write to (real) stdout */
	ssize_t ret;
	while (1) {
		ret = read_to_buf(fd_user_out, buf, sizeof(buf));
		if (!ret)
			break;
		while (ret > 0) {
			ssize_t res = write(model_out, buf, ret);
			if (res < 0) {
				perror("write");
				exit(EXIT_FAILURE);
			}
			ret -= res;
		}
	}

	model_print("---- END PROGRAM OUTPUT   ----\n");
}
#endif /* ! CONFIG_DEBUG */
