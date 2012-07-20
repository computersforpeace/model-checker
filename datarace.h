/** @file datarace.h
 *  @brief Data race detection code.
 */

#ifndef DATARACE_H
#include "config.h"
#include <stdint.h>
#include "clockvector.h"

struct ShadowTable {
	void * array[65536];
};

struct ShadowBaseTable {
	uint64_t array[65536];
};

#define MASK16BIT 0xffff

void initRaceDetector();
void raceCheckWrite(thread_id_t thread, void *location, ClockVector *currClock);
void raceCheckRead(thread_id_t thread, void *location, ClockVector *currClock);



/** Basic encoding idea:
 *	 (void *) Either:
 *	 (1) points to a full record or
 *
 * (2) encodes the information in a 64 bit word.  Encoding is as
 * follows: lowest bit set to 1, next 8 bits are read thread id, next
 * 23 bits are read clock vector, next 8 bites are write thread id,
 * next 23 bits are write clock vector.  */

struct RaceRecord {
	modelclock_t *readClock;
	thread_id_t *thread;
	int capacity;
	int numReads;
	thread_id_t writeThread;
	modelclock_t writeClock;
};

#define INITCAPACITY 4

#define ISSHORTRECORD(x) ((x)&0x1)

#define THREADMASK 0xff
#define RDTHREADID(x) (((x)>>1)&THREADMASK)
#define READMASK 0x07fffff
#define READVECTOR(x) (((x)>>9)&READMASK)

#define WRTHREADID(x) (((x)>>32)&THREADMASK)

#define WRITEMASK READMASK
#define WRITEVECTOR(x) (((x)>>40)&WRITEMASK)

#define ENCODEOP(rdthread, rdtime, wrthread, wrtime) (0x1ULL | ((rdthread)<<1) | ((rdtime) << 9) | (((uint64_t)wrthread)<<32) | (((uint64_t)wrtime)<<40))

#define MAXTHREADID (THREADMASK-1)
#define MAXREADVECTOR (READMASK-1)
#define MAXWRITEVECTOR (WRITEMASK-1)
#endif
