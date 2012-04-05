#include "common.h"

/* for RTLD_NEXT */
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <dlfcn.h>

static void * (*real_malloc)(size_t) = NULL;
static void (*real_free)(void *ptr) = NULL;

static void __my_alloc_init(void)
{

	real_malloc = (void *(*)(size_t))dlsym(RTLD_NEXT, "malloc");
	real_free = (void (*)(void *))dlsym(RTLD_NEXT, "free");
	if (real_malloc == NULL || real_free == NULL) {
		fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
		return;
	}
}

void *myMalloc(size_t size)
{
	if (real_malloc == NULL)
		__my_alloc_init();

	return real_malloc(size);
}

void myFree(void *ptr)
{
	if (real_free == NULL)
		__my_alloc_init();

	real_free(ptr);
}
