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

#define ASSERT(expr) \
do { \
	if (!(expr)) { \
		fprintf(stderr, "Error: assertion failed in %s at line %d\n", __FILE__, __LINE__); \
		exit(EXIT_FAILURE); \
	} \
} while (0);

#endif /* __COMMON_H__ */
