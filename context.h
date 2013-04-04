/**
 * @file context.h
 * @brief ucontext header, since Mac OSX swapcontext() is broken
 */

#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <ucontext.h>

static inline int model_swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
#ifdef MAC
	/*
	 * Mac OSX swapcontext() clobbers some registers, so use a hand-rolled
	 * version with {get,set}context(). We can avoid the same problem
	 * (where optimizations can break the following code) because we don't
	 * statically link with the C library
	 */

	/* volatile, so that 'i' doesn't get promoted to a register */
	volatile int i = 0;

	getcontext(oucp);

	if (i == 0) {
		i = 1;
		setcontext(ucp);
	}

	return 0;
#else
	return swapcontext(oucp, ucp);
#endif
}

#endif /* __CONTEXT_H__ */
