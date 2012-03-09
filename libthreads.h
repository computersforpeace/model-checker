#ifndef __LIBTHREADS_H__
#define __LIBTHREADS_H__

#include <stdio.h>
#include <ucontext.h>

//#define CONFIG_DEBUG

#ifdef CONFIG_DEBUG
#define DEBUG(fmt, ...) do { printf("*** %25s(): line %-4d *** " fmt, __func__, __LINE__, ##__VA_ARGS__); } while (0)
#define DBG() DEBUG("\n");
#else
#define DEBUG(fmt, ...)
#define DBG()
#endif

struct thread {
	void (*start_routine);
	void *arg;
	ucontext_t context;
	void *stack;
	int index;
	int completed;
};

int thread_create(struct thread *t, void (*start_routine), void *arg);
void thread_join(struct thread *t);

#endif /* __LIBTHREADS_H__ */
