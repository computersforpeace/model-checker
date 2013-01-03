#include <inttypes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <ucontext.h>

#include "hashtable.h"
#include "snapshot.h"
#include "mymemory.h"
#include "common.h"

#define FAILURE(mesg) { model_print("failed in the API: %s with errno relative message: %s\n", mesg, strerror(errno)); exit(EXIT_FAILURE); }

/** PageAlignedAdressUpdate return a page aligned address for the
 * address being added as a side effect the numBytes are also changed.
 */
static void * PageAlignAddressUpward(void *addr)
{
	return (void *)((((uintptr_t)addr) + PAGESIZE - 1) & ~(PAGESIZE - 1));
}

#if USE_MPROTECT_SNAPSHOT

/* Each SnapShotRecord lists the firstbackingpage that must be written to
 * revert to that snapshot */
struct SnapShotRecord {
	unsigned int firstBackingPage;
};

/** @brief Backing store page */
typedef unsigned char snapshot_page_t[PAGESIZE];

/* List the base address of the corresponding page in the backing store so we
 * know where to copy it to */
struct BackingPageRecord {
	void *basePtrOfPage;
};

/* Struct for each memory region */
struct MemoryRegion {
	void *basePtr; // base of memory region
	int sizeInPages; // size of memory region in pages
};

/* Primary struct for snapshotting system */
struct mprot_snapshotter {
	struct MemoryRegion *regionsToSnapShot; //This pointer references an array of memory regions to snapshot
	snapshot_page_t *backingStore; //This pointer references an array of snapshotpage's that form the backing store
	void *backingStoreBasePtr; //This pointer references an array of snapshotpage's that form the backing store
	struct BackingPageRecord *backingRecords; //This pointer references an array of backingpagerecord's (same number of elements as backingstore
	struct SnapShotRecord *snapShots; //This pointer references the snapshot array

	unsigned int lastSnapShot; //Stores the next snapshot record we should use
	unsigned int lastBackingPage; //Stores the next backingpage we should use
	unsigned int lastRegion; //Stores the next memory region to be used

	unsigned int maxRegions; //Stores the max number of memory regions we support
	unsigned int maxBackingPages; //Stores the total number of backing pages
	unsigned int maxSnapShots; //Stores the total number of snapshots we allow
};

static struct mprot_snapshotter *mprot_snap = NULL;

/** ReturnPageAlignedAddress returns a page aligned address for the
 * address being added as a side effect the numBytes are also changed.
 */
static void * ReturnPageAlignedAddress(void *addr)
{
	return (void *)(((uintptr_t)addr) & ~(PAGESIZE - 1));
}

/** The initSnapShotRecord method initialized the snapshotting data
 *  structures for the mprotect based snapshot.
 */
static void initSnapShotRecord(unsigned int numbackingpages, unsigned int numsnapshots, unsigned int nummemoryregions)
{
	mprot_snap = (struct mprot_snapshotter *)model_malloc(sizeof(struct mprot_snapshotter));
	mprot_snap->regionsToSnapShot = (struct MemoryRegion *)model_malloc(sizeof(struct MemoryRegion) * nummemoryregions);
	mprot_snap->backingStoreBasePtr = (void *)model_malloc(sizeof(snapshot_page_t) * (numbackingpages + 1));
	//Page align the backingstorepages
	mprot_snap->backingStore = (snapshot_page_t *)PageAlignAddressUpward(mprot_snap->backingStoreBasePtr);
	mprot_snap->backingRecords = (struct BackingPageRecord *)model_malloc(sizeof(struct BackingPageRecord) * numbackingpages);
	mprot_snap->snapShots = (struct SnapShotRecord *)model_malloc(sizeof(struct SnapShotRecord) * numsnapshots);
	mprot_snap->lastSnapShot = 0;
	mprot_snap->lastBackingPage = 0;
	mprot_snap->lastRegion = 0;
	mprot_snap->maxRegions = nummemoryregions;
	mprot_snap->maxBackingPages = numbackingpages;
	mprot_snap->maxSnapShots = numsnapshots;
}

/** HandlePF is the page fault handler for mprotect based snapshotting
 * algorithm.
 */
static void HandlePF(int sig, siginfo_t *si, void *unused)
{
	if (si->si_code == SEGV_MAPERR) {
		model_print("Real Fault at %p\n", si->si_addr);
		print_trace();
		model_print("For debugging, place breakpoint at: %s:%d\n",
				__FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
	void* addr = ReturnPageAlignedAddress(si->si_addr);

	unsigned int backingpage = mprot_snap->lastBackingPage++; //Could run out of pages...
	if (backingpage == mprot_snap->maxBackingPages) {
		model_print("Out of backing pages at %p\n", si->si_addr);
		exit(EXIT_FAILURE);
	}

	//copy page
	memcpy(&(mprot_snap->backingStore[backingpage]), addr, sizeof(snapshot_page_t));
	//remember where to copy page back to
	mprot_snap->backingRecords[backingpage].basePtrOfPage = addr;
	//set protection to read/write
	if (mprotect(addr, sizeof(snapshot_page_t), PROT_READ | PROT_WRITE)) {
		perror("mprotect");
		// Handle error by quitting?
	}
}

static void mprot_snapshot_init(unsigned int numbackingpages,
		unsigned int numsnapshots, unsigned int nummemoryregions,
		unsigned int numheappages, VoidFuncPtr entryPoint)
{
	/* Setup a stack for our signal handler....  */
	stack_t ss;
	ss.ss_sp = PageAlignAddressUpward(model_malloc(SIGSTACKSIZE + PAGESIZE - 1));
	ss.ss_size = SIGSTACKSIZE;
	ss.ss_flags = 0;
	sigaltstack(&ss, NULL);

	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_RESTART | SA_ONSTACK;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = HandlePF;
#ifdef MAC
	if (sigaction(SIGBUS, &sa, NULL) == -1) {
		model_print("SIGACTION CANNOT BE INSTALLED\n");
		exit(EXIT_FAILURE);
	}
#endif
	if (sigaction(SIGSEGV, &sa, NULL) == -1) {
		model_print("SIGACTION CANNOT BE INSTALLED\n");
		exit(EXIT_FAILURE);
	}

	initSnapShotRecord(numbackingpages, numsnapshots, nummemoryregions);

	// EVIL HACK: We need to make sure that calls into the HandlePF method don't cause dynamic links
	// The problem is that we end up protecting state in the dynamic linker...
	// Solution is to call our signal handler before we start protecting stuff...

	siginfo_t si;
	memset(&si, 0, sizeof(si));
	si.si_addr = ss.ss_sp;
	HandlePF(SIGSEGV, &si, NULL);
	mprot_snap->lastBackingPage--; //remove the fake page we copied

	void *basemySpace = model_malloc((numheappages + 1) * PAGESIZE);
	void *pagealignedbase = PageAlignAddressUpward(basemySpace);
	user_snapshot_space = create_mspace_with_base(pagealignedbase, numheappages * PAGESIZE, 1);
	addMemoryRegionToSnapShot(pagealignedbase, numheappages);

	void *base_model_snapshot_space = model_malloc((numheappages + 1) * PAGESIZE);
	pagealignedbase = PageAlignAddressUpward(base_model_snapshot_space);
	model_snapshot_space = create_mspace_with_base(pagealignedbase, numheappages * PAGESIZE, 1);
	addMemoryRegionToSnapShot(pagealignedbase, numheappages);

	entryPoint();
}

static void mprot_add_to_snapshot(void *addr, unsigned int numPages)
{
	unsigned int memoryregion = mprot_snap->lastRegion++;
	if (memoryregion == mprot_snap->maxRegions) {
		model_print("Exceeded supported number of memory regions!\n");
		exit(EXIT_FAILURE);
	}

	mprot_snap->regionsToSnapShot[memoryregion].basePtr = addr;
	mprot_snap->regionsToSnapShot[memoryregion].sizeInPages = numPages;
}

static snapshot_id mprot_take_snapshot()
{
	for (unsigned int region = 0; region < mprot_snap->lastRegion; region++) {
		if (mprotect(mprot_snap->regionsToSnapShot[region].basePtr, mprot_snap->regionsToSnapShot[region].sizeInPages * sizeof(snapshot_page_t), PROT_READ) == -1) {
			perror("mprotect");
			model_print("Failed to mprotect inside of takeSnapShot\n");
			exit(EXIT_FAILURE);
		}
	}
	unsigned int snapshot = mprot_snap->lastSnapShot++;
	if (snapshot == mprot_snap->maxSnapShots) {
		model_print("Out of snapshots\n");
		exit(EXIT_FAILURE);
	}
	mprot_snap->snapShots[snapshot].firstBackingPage = mprot_snap->lastBackingPage;

	return snapshot;
}

static void mprot_roll_back(snapshot_id theID)
{
#if USE_MPROTECT_SNAPSHOT == 2
	if (mprot_snap->lastSnapShot == (theID + 1)) {
		for (unsigned int page = mprot_snap->snapShots[theID].firstBackingPage; page < mprot_snap->lastBackingPage; page++) {
			memcpy(mprot_snap->backingRecords[page].basePtrOfPage, &mprot_snap->backingStore[page], sizeof(snapshot_page_t));
		}
		return;
	}
#endif

	HashTable< void *, bool, uintptr_t, 4, model_malloc, model_calloc, model_free> duplicateMap;
	for (unsigned int region = 0; region < mprot_snap->lastRegion; region++) {
		if (mprotect(mprot_snap->regionsToSnapShot[region].basePtr, mprot_snap->regionsToSnapShot[region].sizeInPages * sizeof(snapshot_page_t), PROT_READ | PROT_WRITE) == -1) {
			perror("mprotect");
			model_print("Failed to mprotect inside of takeSnapShot\n");
			exit(EXIT_FAILURE);
		}
	}
	for (unsigned int page = mprot_snap->snapShots[theID].firstBackingPage; page < mprot_snap->lastBackingPage; page++) {
		if (!duplicateMap.contains(mprot_snap->backingRecords[page].basePtrOfPage)) {
			duplicateMap.put(mprot_snap->backingRecords[page].basePtrOfPage, true);
			memcpy(mprot_snap->backingRecords[page].basePtrOfPage, &mprot_snap->backingStore[page], sizeof(snapshot_page_t));
		}
	}
	mprot_snap->lastSnapShot = theID;
	mprot_snap->lastBackingPage = mprot_snap->snapShots[theID].firstBackingPage;
	mprot_take_snapshot(); //Make sure current snapshot is still good...All later ones are cleared
}

#else /* !USE_MPROTECT_SNAPSHOT */

#include <ucontext.h>

#define SHARED_MEMORY_DEFAULT  (100 * ((size_t)1 << 20)) // 100mb for the shared memory
#define STACK_SIZE_DEFAULT      (((size_t)1 << 20) * 20)  // 20 mb out of the above 100 mb for my stack

struct fork_snapshotter {
	void *mSharedMemoryBase;
	void *mStackBase;
	size_t mStackSize;
	volatile snapshot_id mIDToRollback;
	ucontext_t mContextToRollback;
	snapshot_id currSnapShotID;
};

static struct fork_snapshotter *fork_snap = NULL;

/** @statics
*   These variables are necessary because the stack is shared region and
*   there exists a race between all processes executing the same function.
*   To avoid the problem above, we require variables allocated in 'safe' regions.
*   The bug was actually observed with the forkID, these variables below are
*   used to indicate the various contexts to which to switch to.
*
*   @savedSnapshotContext: contains the point to which takesnapshot() call should switch to.
*   @savedUserSnapshotContext: contains the point to which the process whose snapshotid is equal to the rollbackid should switch to
*   @snapshotid: it is a running counter for the various forked processes snapshotid. it is incremented and set in a persistently shared record
*/
static ucontext_t savedSnapshotContext;
static ucontext_t savedUserSnapshotContext;
static snapshot_id snapshotid = 0;

static void createSharedMemory()
{
	//step 1. create shared memory.
	void *memMapBase = mmap(0, SHARED_MEMORY_DEFAULT + STACK_SIZE_DEFAULT, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	if (MAP_FAILED == memMapBase)
		FAILURE("mmap");

	//Setup snapshot record at top of free region
	fork_snap = (struct fork_snapshotter *)memMapBase;
	fork_snap->mSharedMemoryBase = (void *)((uintptr_t)memMapBase + sizeof(struct fork_snapshotter));
	fork_snap->mStackBase = (void *)((uintptr_t)memMapBase + SHARED_MEMORY_DEFAULT);
	fork_snap->mStackSize = STACK_SIZE_DEFAULT;
	fork_snap->mIDToRollback = -1;
	fork_snap->currSnapShotID = 0;
}

/**
 * Create a new mspace pointer for the non-snapshotting (i.e., inter-process
 * shared) memory region. Only for fork-based snapshotting.
 *
 * @return The shared memory mspace
 */
mspace create_shared_mspace()
{
	if (!fork_snap)
		createSharedMemory();
	return create_mspace_with_base((void *)(fork_snap->mSharedMemoryBase), SHARED_MEMORY_DEFAULT - sizeof(struct fork_snapshotter), 1);
}

static void fork_snapshot_init(unsigned int numbackingpages,
		unsigned int numsnapshots, unsigned int nummemoryregions,
		unsigned int numheappages, VoidFuncPtr entryPoint)
{
	if (!fork_snap)
		createSharedMemory();

	void *base_model_snapshot_space = malloc((numheappages + 1) * PAGESIZE);
	void *pagealignedbase = PageAlignAddressUpward(base_model_snapshot_space);
	model_snapshot_space = create_mspace_with_base(pagealignedbase, numheappages * PAGESIZE, 1);

	//step 2 setup the stack context.
	ucontext_t newContext;
	getcontext(&newContext);
	newContext.uc_stack.ss_sp = fork_snap->mStackBase;
	newContext.uc_stack.ss_size = STACK_SIZE_DEFAULT;
	makecontext(&newContext, entryPoint, 0);
	/* switch to a new entryPoint context, on a new stack */
	swapcontext(&savedSnapshotContext, &newContext);

	/* switch back here when takesnapshot is called */
	pid_t forkedID = 0;
	snapshotid = fork_snap->currSnapShotID;
	/* This bool indicates that the current process's snapshotid is same
		 as the id to which the rollback needs to occur */

	bool rollback = false;
	while (true) {
		fork_snap->currSnapShotID = snapshotid + 1;
		forkedID = fork();

		if (0 == forkedID) {
			/* If the rollback bool is set, switch to the context we need to
				 return to during a rollback. */
			if (rollback) {
				setcontext(&(fork_snap->mContextToRollback));
			} else {
				/*Child process which is forked as a result of takesnapshot
					call should switch back to the takesnapshot context*/
				setcontext(&savedUserSnapshotContext);
			}
		} else {
			int status;
			int retVal;

			DEBUG("The process id of child is %d and the process id of this process is %d and snapshot id is %d\n",
			        forkedID, getpid(), snapshotid);

			do {
				retVal = waitpid(forkedID, &status, 0);
			} while (-1 == retVal && errno == EINTR);

			if (fork_snap->mIDToRollback != snapshotid) {
				exit(EXIT_SUCCESS);
			}
			rollback = true;
		}
	}
}

static snapshot_id fork_take_snapshot()
{
	swapcontext(&savedUserSnapshotContext, &savedSnapshotContext);
	DEBUG("TAKESNAPSHOT RETURN\n");
	return snapshotid;
}

static void fork_roll_back(snapshot_id theID)
{
	fork_snap->mIDToRollback = theID;
	volatile int sTemp = 0;
	getcontext(&fork_snap->mContextToRollback);
	/*
	 * This is used to quit the process on rollback, so that the process
	 * which needs to rollback can quit allowing the process whose
	 * snapshotid matches the rollbackid to switch to this context and
	 * continue....
	 */
	if (!sTemp) {
		sTemp = 1;
		DEBUG("Invoked rollback\n");
		exit(EXIT_SUCCESS);
	}
	/*
	 * This fix obviates the need for a finalize call. hence less dependences for model-checker....
	 */
	fork_snap->mIDToRollback = -1;
}

#endif /* !USE_MPROTECT_SNAPSHOT */

/** The initSnapshotLibrary function initializes the snapshot library.
 *  @param entryPoint the function that should run the program.
 */
void initSnapshotLibrary(unsigned int numbackingpages,
		unsigned int numsnapshots, unsigned int nummemoryregions,
		unsigned int numheappages, VoidFuncPtr entryPoint)
{
#if USE_MPROTECT_SNAPSHOT
	mprot_snapshot_init(numbackingpages, numsnapshots, nummemoryregions, numheappages, entryPoint);
#else
	fork_snapshot_init(numbackingpages, numsnapshots, nummemoryregions, numheappages, entryPoint);
#endif
}

/** The addMemoryRegionToSnapShot function assumes that addr is page aligned. */
void addMemoryRegionToSnapShot(void *addr, unsigned int numPages)
{
#if USE_MPROTECT_SNAPSHOT
	mprot_add_to_snapshot(addr, numPages);
#else
	/* not needed for fork-based snapshotting */
#endif
}

/** The takeSnapshot function takes a snapshot.
 * @return The snapshot identifier.
 */
snapshot_id takeSnapshot()
{
#if USE_MPROTECT_SNAPSHOT
	return mprot_take_snapshot();
#else
	return fork_take_snapshot();
#endif
}

/** The rollBack function rollback to the given snapshot identifier.
 *  @param theID is the snapshot identifier to rollback to.
 */
void rollBack(snapshot_id theID)
{
#if USE_MPROTECT_SNAPSHOT
	mprot_roll_back(theID);
#else
	fork_roll_back(theID);
#endif
}
