#include "context.h"

#ifdef MAC

int model_swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
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
}

#endif /* MAC */
