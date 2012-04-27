#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>

//#define CONFIG_DEBUG

#ifdef CONFIG_DEBUG
#define DEBUG(fmt, ...) do { printf("*** %25s(): line %-4d *** " fmt, __func__, __LINE__, ##__VA_ARGS__); } while (0)
#define DBG() DEBUG("\n");
#define DBG_ENABLED() (1)
#else
#define DEBUG(fmt, ...)
#define DBG()
#define DBG_ENABLED() (0)
#endif

void * myMalloc(size_t size);
void myFree(void *ptr);

#define userMalloc(size)	malloc(size)
#define userFree(ptr)		free(ptr)

#endif /* __COMMON_H__ */
