#ifndef __LIBTHREADS_H__
#define __LIBTHREADS_H__

#ifdef __cplusplus
extern "C" {
#endif

	typedef int thread_id_t;
	typedef void (*thrd_start_t)();

	typedef thread_id_t thrd_t;

	int thrd_create(thrd_t *t, thrd_start_t start_routine, void *arg);
	int thrd_join(thrd_t);
	int thrd_yield(void);
	thrd_t thrd_current(void);

	extern void user_main(void);

#ifdef __cplusplus
}
#endif

#endif /* __LIBTHREADS_H__ */
