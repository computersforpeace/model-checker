/** @file common.h
 *  @brief General purpose macros.
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include "config.h"

#ifdef CONFIG_DEBUG
#define DEBUG(fmt, ...) do { printf("*** %25s(): line %-4d *** " fmt, __func__, __LINE__, ##__VA_ARGS__); } while (0)
#define DBG() DEBUG("\n");
#define DBG_ENABLED() (1)
#else
#define DEBUG(fmt, ...)
#define DBG()
#define DBG_ENABLED() (0)
#endif

void assert_hook(void);

#define ASSERT(expr) \
do { \
	if (!(expr)) { \
		fprintf(stderr, "Error: assertion failed in %s at line %d\n", __FILE__, __LINE__); \
		print_trace(); \
		model_print_summary(); \
		assert_hook();				 \
		exit(EXIT_FAILURE); \
	} \
} while (0);

#define error_msg(...) fprintf(stderr, "Error: " __VA_ARGS__)

void print_trace(void);
void model_print_summary(void);
#endif /* __COMMON_H__ */
