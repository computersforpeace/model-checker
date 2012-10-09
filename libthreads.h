/** @file libthreads.h
 *  @brief Basic Thread Library Functionality.
 */

#ifndef __LIBTHREADS_H__
#define __LIBTHREADS_H__

#ifdef __cplusplus
extern "C" {
#endif

	typedef void (*thrd_start_t)(void *);

	typedef int thrd_t;

	int thrd_create(thrd_t *t, thrd_start_t start_routine, void *arg);
	int thrd_join(thrd_t);
	int thrd_yield(void);
	thrd_t thrd_current(void);

	int user_main(int, char**);

#ifdef __cplusplus
}
#endif

#endif /* __LIBTHREADS_H__ */
