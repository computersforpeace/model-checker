/** @file threads.h
 *  @brief C11 Thread Library Functionality
 */

#ifndef __THREADS_H__
#define __THREADS_H__

/* Forward declaration */
struct Thread; /* actually, class; but this is safe */

#ifdef __cplusplus
extern "C" {
#endif

	typedef void (*thrd_start_t)(void *);

	typedef struct {
		struct Thread *priv;
	} thrd_t;

	int thrd_create(thrd_t *t, thrd_start_t start_routine, void *arg);
	int thrd_join(thrd_t);
	void thrd_yield(void);
	thrd_t thrd_current(void);

	int user_main(int, char**);

#ifdef __cplusplus
}
#endif

#endif /* __THREADS_H__ */
