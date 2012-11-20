/** @file output.h
 *  @brief Functions for redirecting program output
 */

#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include "config.h"

#ifdef CONFIG_DEBUG
static inline void redirect_output() { }
static inline void clear_program_output() { }
static inline void print_program_output() { }
#else
void redirect_output();
void clear_program_output();
void print_program_output();
#endif /* ! CONFIG_DEBUG */

#endif /* __OUTPUT_H__ */
