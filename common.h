#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdio.h>

//#define CONFIG_DEBUG

#ifdef CONFIG_DEBUG
#define DEBUG(fmt, ...) do { printf("*** %25s(): line %-4d *** " fmt, __func__, __LINE__, ##__VA_ARGS__); } while (0)
#define DBG() DEBUG("\n");
#else
#define DEBUG(fmt, ...)
#define DBG()
#endif

#endif /* __CONFIG_H__ */
