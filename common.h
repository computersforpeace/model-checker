#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>

//#define CONFIG_DEBUG

#ifdef CONFIG_DEBUG
#define DEBUG(fmt, ...) do { printf("*** %25s(): line %-4d *** " fmt, __func__, __LINE__, ##__VA_ARGS__); } while (0)
#define DBG() DEBUG("\n");
#else
#define DEBUG(fmt, ...)
#define DBG()
#endif

void *myMalloc(size_t size);
void myFree(void *ptr);

#endif /* __COMMON_H__ */
