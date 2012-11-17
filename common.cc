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

FILE *model_out;
int fd_user_out; /**< @brief File descriptor from which to read user program output */

#define CONFIG_STACKTRACE
/** Print a backtrace of the current program state. */
void print_trace(void)
{
#ifdef CONFIG_STACKTRACE
	print_stacktrace(stdout);
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

void model_print_summary(void)
{
	model->print_summary();
}

void assert_hook(void)
{
	model_print("Add breakpoint to line %u in file %s.\n",__LINE__,__FILE__);
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

/**
 * @brief Setup output redirecting
 *
 * Redirects user program's stdout to a pipe so that we can dump it
 * selectively, when displaying bugs, etc.
 * Also connects a special file 'model_out' directly to stdout, for printing
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
 */
void redirect_output()
{
	int fd;

	/* Save stdout for later use */
	fd = dup(fileno(stdout));
	model_out = fdopen(fd, "w");

	/* Redirect program output to a pipe */
	int pipefd[2];
	if (pipe(pipefd) < 0) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}
	fd = dup2(pipefd[1], fileno(stdout)); // STDOUT_FILENO
	close(pipefd[1]);

	/* Save the "read" side of the pipe for use later */
	fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
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

	/* Gather all program output */
	fflush(stdout);

	/* Read program output pipe and write to (real) stdout */
	int ret;
	while (1) {
		ret = read_to_buf(fd_user_out, buf, sizeof(buf));
		if (!ret)
			break;
		while (ret > 0) {
			int res = fwrite(buf, 1, ret, model_out);
			if (res < 0) {
				errno = ferror(model_out);
				perror("fwrite");
				exit(EXIT_FAILURE);
			}
			ret -= res;
		}
	}
}
