/**
 * @file modeltypes.h
 * @brief Common typedefs for the model-checker
 */

#ifndef __MODELTYPES_H__
#define __MODELTYPES_H__

/**
 * @brief Represents a unique ID for a Thread
 *
 * The space of unique IDs may need to become a non-compact
 * or non-zero-indexed set of integers (or even some other
 * type). So this typedef is used to help identify which is
 * which, where a simple 'int' is meant to be a compact,
 * zero-indexed set and a 'thread_id_t' may be another type
 * entirely.
 *
 * @see id_to_int
 * @see int_to_id
 */
typedef int thread_id_t;

#define THREAD_ID_T_NONE	-1

typedef unsigned int modelclock_t;

#endif /* __MODELTYPES_H__ */
