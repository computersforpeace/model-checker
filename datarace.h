/** @file datarace.h
 *  @brief Data race detection code.
 */

#ifndef __DATARACE_H__
#define __DATARACE_H__

#include "config.h"
#include <stdint.h>
#include "modeltypes.h"

/* Forward declaration */
class ModelAction;

struct ShadowTable {
	void * array[65536];
};

struct ShadowBaseTable {
	uint64_t array[65536];
};

struct DataRace {
	/* Clock and thread associated with first action.  This won't change in
		 response to synchronization. */

	thread_id_t oldthread;
	modelclock_t oldclock;
	/* Record whether this is a write, so we can tell the user. */
	bool isoldwrite;

	/* Model action associated with second action.  This could change as
		 a result of synchronization. */
	ModelAction *newaction;
	/* Record whether this is a write, so we can tell the user. */
	bool isnewwrite;

	/* Address of data race. */
	const void *address;
};

#define MASK16BIT 0xffff

void initRaceDetector();
void raceCheckWrite(thread_id_t thread, void *location);
void raceCheckRead(thread_id_t thread, const void *location);
bool checkDataRaces();
void assert_race(struct DataRace *race);
bool haveUnrealizedRaces();

/**
 * @brief A record of information for detecting data races
 */
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

/**
 * The basic encoding idea is that (void *) either:
 *  -# points to a full record (RaceRecord) or
 *  -# encodes the information in a 64 bit word. Encoding is as
 *     follows:
 *     - lowest bit set to 1
 *     - next 8 bits are read thread id
 *     - next 23 bits are read clock vector
 *     - next 8 bits are write thread id
 *     - next 23 bits are write clock vector
 */
#define ENCODEOP(rdthread, rdtime, wrthread, wrtime) (0x1ULL | ((rdthread)<<1) | ((rdtime) << 9) | (((uint64_t)wrthread)<<32) | (((uint64_t)wrtime)<<40))

#define MAXTHREADID (THREADMASK-1)
#define MAXREADVECTOR (READMASK-1)
#define MAXWRITEVECTOR (WRITEMASK-1)

#endif /* __DATARACE_H__ */
