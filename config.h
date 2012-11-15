/** @file config.h
 * @brief Configuration file.
 */

#ifndef CONFIG_H
#define CONFIG_H

/** Turn on debugging. */
/*		#ifndef CONFIG_DEBUG
		#define CONFIG_DEBUG
		#endif

		#ifndef CONFIG_ASSERT
		#define CONFIG_ASSERT
		#endif
*/

/** Turn on support for dumping cyclegraphs as dot files at each
 *  printed summary.*/
#define SUPPORT_MOD_ORDER_DUMP 0

/** Do we have a 48 bit virtual address (64 bit machine) or 32 bit addresses.
 * Set to 1 for 48-bit, 0 for 32-bit. */
#ifndef BIT48
#ifdef _LP64
#define BIT48 1
#else
#define BIT48 0
#endif
#endif /* BIT48 */

/** Snapshotting configurables */

/** 
 * If USE_MPROTECT_SNAPSHOT=2, then snapshot by tuned mmap() algorithm
 * If USE_MPROTECT_SNAPSHOT=1, then snapshot by using mmap() and mprotect()
 * If USE_MPROTECT_SNAPSHOT=0, then snapshot by using fork() */
#define USE_MPROTECT_SNAPSHOT 2

/** Size of signal stack */
#define SIGSTACKSIZE 65536

/** Page size configuration */
#define PAGESIZE 4096

/** Thread parameters */

/* Size of stack to allocate for a thread. */
#define STACK_SIZE (1024 * 1024)

/** How many shadow tables of memory to preallocate for data race detector. */
#define SHADOWBASETABLES 4

/** Enable debugging assertions (via ASSERT()) */
#define CONFIG_ASSERT

#endif
