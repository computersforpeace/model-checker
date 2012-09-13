#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define MAX_TRACE_LEN 100

/** Print a backtrace of the current program state. */
void print_trace(void)
{
	void *array[MAX_TRACE_LEN];
	char **strings;
	int size, i;

	size = backtrace(array, MAX_TRACE_LEN);
	strings = backtrace_symbols(array, size);

	printf("\nDumping stack trace (%d frames):\n", size);

	for (i = 0; i < size; i++)
		printf("\t%s\n", strings[i]);

	free(strings);
}
