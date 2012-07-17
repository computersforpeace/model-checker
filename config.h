/** @file config.h
 * @brief Configuration file.
 */

#ifndef CONFIG_H
#define CONFIG_H

/** Do we have a 48 bit virtual address (64 bit machine) or 32 bit addresses.
 * Set to 1 for 48-bit, 0 for 32-bit. */
#ifndef BIT48

#ifdef _LP64
#define BIT48 1
#else
#define BIT48 0
#endif

#endif /* BIT48 */

#endif
