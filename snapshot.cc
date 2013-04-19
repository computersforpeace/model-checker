#include <inttypes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#include "hashtable.h"
#include "snapshot.h"
#include "mymemory.h"
#include "common.h"
#include "context.h"

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

/** ReturnPageAlignedAddress returns a page aligned address for the
 * address being added as a side effect the numBytes are also changed.
 */
static void * ReturnPageAlignedAddress(void *addr)
{
	return (void *)(((uintptr_t)addr) & ~(PAGESIZE - 1));
}

/* Primary struct for snapshotting system */
struct mprot_snapshotter {
	mprot_snapshotter(unsigned int numbackingpages, unsigned int numsnapshots, unsigned int nummemoryregions);
	~mprot_snapshotter();

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

	MEMALLOC
};

static struct mprot_snapshotter *mprot_snap = NULL;

mprot_snapshotter::mprot_snapshotter(unsigned int backing_pages, unsigned int snapshots, unsigned int regions) :
	lastSnapShot(0),
	lastBackingPage(0),
	lastRegion(0),
	maxRegions(regions),
	maxBackingPages(backing_pages),
	maxSnapShots(snapshots)
{
	regionsToSnapShot = (struct MemoryRegion *)model_malloc(sizeof(struct MemoryRegion) * regions);
	backingStoreBasePtr = (void *)model_malloc(sizeof(snapshot_page_t) * (backing_pages + 1));
	//Page align the backingstorepages
	backingStore = (snapshot_page_t *)PageAlignAddressUpward(backingStoreBasePtr);
	backingRecords = (struct BackingPageRecord *)model_malloc(sizeof(struct BackingPageRecord) * backing_pages);
	snapShots = (struct SnapShotRecord *)model_malloc(sizeof(struct SnapShotRecord) * snapshots);
}

mprot_snapshotter::~mprot_snapshotter()
{
	model_free(regionsToSnapShot);
	model_free(backingStoreBasePtr);
	model_free(backingRecords);
	model_free(snapShots);
}

/** mprot_handle_pf is the page fault handler for mprotect based snapshotting
 * algorithm.
 */
static void mprot_handle_pf(int sig, siginfo_t *si, void *unused)
{
	if (si->si_code == SEGV_MAPERR) {
		model_print("Segmentation fault at %p\n", si->si_addr);
		model_print("For debugging, place breakpoint at: %s:%d\n",
				__FILE__, __LINE__);
		// print_trace(); // Trace printing may cause dynamic memory allocation
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
	sa.sa_sigaction = mprot_handle_pf;
#ifdef MAC
	if (sigaction(SIGBUS, &sa, NULL) == -1) {
		perror("sigaction(SIGBUS)");
		exit(EXIT_FAILURE);
	}
#endif
	if (sigaction(SIGSEGV, &sa, NULL) == -1) {
		perror("sigaction(SIGSEGV)");
		exit(EXIT_FAILURE);
	}

	mprot_snap = new mprot_snapshotter(numbackingpages, numsnapshots, nummemoryregions);

	// EVIL HACK: We need to make sure that calls into the mprot_handle_pf method don't cause dynamic links
	// The problem is that we end up protecting state in the dynamic linker...
	// Solution is to call our signal handler before we start protecting stuff...

	siginfo_t si;
	memset(&si, 0, sizeof(si));
	si.si_addr = ss.ss_sp;
	mprot_handle_pf(SIGSEGV, &si, NULL);
	mprot_snap->lastBackingPage--; //remove the fake page we copied

	void *basemySpace = model_malloc((numheappages + 1) * PAGESIZE);
	void *pagealignedbase = PageAlignAddressUpward(basemySpace);
	user_snapshot_space = create_mspace_with_base(pagealignedbase, numheappages * PAGESIZE, 1);
	snapshot_add_memory_region(pagealignedbase, numheappages);

	void *base_model_snapshot_space = model_malloc((numheappages + 1) * PAGESIZE);
	pagealignedbase = PageAlignAddressUpward(base_model_snapshot_space);
	model_snapshot_space = create_mspace_with_base(pagealignedbase, numheappages * PAGESIZE, 1);
	snapshot_add_memory_region(pagealignedbase, numheappages);

	entryPoint();
}

static void mprot_add_to_snapshot(void *addr, unsigned int numPages)
{
	unsigned int memoryregion = mprot_snap->lastRegion++;
	if (memoryregion == mprot_snap->maxRegions) {
		model_print("Exceeded supported number of memory regions!\n");
		exit(EXIT_FAILURE);
	}

	DEBUG("snapshot region %p-%p (%u page%s)\n",
			addr, (char *)addr + numPages * PAGESIZE, numPages,
			numPages > 1 ? "s" : "");
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

#define SHARED_MEMORY_DEFAULT  (100 * ((size_t)1 << 20)) // 100mb for the shared memory
#define STACK_SIZE_DEFAULT      (((size_t)1 << 20) * 20)  // 20 mb out of the above 100 mb for my stack

struct fork_snapshotter {
	/** @brief Pointer to the shared (non-snapshot) memory heap base
	 * (NOTE: this has size SHARED_MEMORY_DEFAULT - sizeof(*fork_snap)) */
	void *mSharedMemoryBase;

	/** @brief Pointer to the shared (non-snapshot) stack region */
	void *mStackBase;

	/** @brief Size of the shared stack */
	size_t mStackSize;

	/**
	 * @brief Stores the ID that we are attempting to roll back to
	 *
	 * Used in inter-process communication so that each process can
	 * determine whether or not to take over execution (w/ matching ID) or
	 * exit (we're rolling back even further). Dubiously marked 'volatile'
	 * to prevent compiler optimizations from messing with the
	 * inter-process behavior.
	 */
	volatile snapshot_id mIDToRollback;

	/**
	 * @brief The context for the shared (non-snapshot) stack
	 *
	 * This context is passed between the various processes which represent
	 * various snapshot states. It should be used primarily for the
	 * "client-side" code, not the main snapshot loop.
	 */
	ucontext_t shared_ctxt;

	/** @brief Inter-process tracking of the next snapshot ID */
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
*   @private_ctxt: the context which is internal to the current process. Used
*   for running the internal snapshot/rollback loop.
*   @exit_ctxt: a special context used just for exiting from a process (so we
*   can use swapcontext() instead of setcontext() + hacks)
*   @snapshotid: it is a running counter for the various forked processes
*   snapshotid. it is incremented and set in a persistently shared record
*/
static ucontext_t private_ctxt;
static ucontext_t exit_ctxt;
static snapshot_id snapshotid = 0;

/**
 * @brief Create a new context, with a given stack and entry function
 * @param ctxt The context structure to fill
 * @param stack The stack to run the new context in
 * @param stacksize The size of the stack
 * @param func The entry point function for the context
 */
static void create_context(ucontext_t *ctxt, void *stack, size_t stacksize,
		void (*func)(void))
{
	getcontext(ctxt);
	ctxt->uc_stack.ss_sp = stack;
	ctxt->uc_stack.ss_size = stacksize;
	makecontext(ctxt, func, 0);
}

/** @brief An empty function, used for an "empty" context which just exits a
 *  process */
static void fork_exit()
{
	/* Intentionally empty */
}

static void createSharedMemory()
{
	//step 1. create shared memory.
	void *memMapBase = mmap(0, SHARED_MEMORY_DEFAULT + STACK_SIZE_DEFAULT, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	if (memMapBase == MAP_FAILED) {
		perror("mmap");
		exit(EXIT_FAILURE);
	}

	//Setup snapshot record at top of free region
	fork_snap = (struct fork_snapshotter *)memMapBase;
	fork_snap->mSharedMemoryBase = (void *)((uintptr_t)memMapBase + sizeof(*fork_snap));
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
	return create_mspace_with_base((void *)(fork_snap->mSharedMemoryBase), SHARED_MEMORY_DEFAULT - sizeof(*fork_snap), 1);
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

	/* setup an "exiting" context */
	char stack[128];
	create_context(&exit_ctxt, stack, sizeof(stack), fork_exit);

	/* setup the shared-stack context */
	create_context(&fork_snap->shared_ctxt, fork_snap->mStackBase,
			STACK_SIZE_DEFAULT, entryPoint);
	/* switch to a new entryPoint context, on a new stack */
	model_swapcontext(&private_ctxt, &fork_snap->shared_ctxt);

	/* switch back here when takesnapshot is called */
	snapshotid = fork_snap->currSnapShotID;

	while (true) {
		pid_t forkedID;
		fork_snap->currSnapShotID = snapshotid + 1;
		forkedID = fork();

		if (0 == forkedID) {
			setcontext(&fork_snap->shared_ctxt);
		} else {
			DEBUG("parent PID: %d, child PID: %d, snapshot ID: %d\n",
			        getpid(), forkedID, snapshotid);

			while (waitpid(forkedID, NULL, 0) < 0) {
				/* waitpid() may be interrupted */
				if (errno != EINTR) {
					perror("waitpid");
					exit(EXIT_FAILURE);
				}
			}

			if (fork_snap->mIDToRollback != snapshotid)
				exit(EXIT_SUCCESS);
		}
	}
}

static snapshot_id fork_take_snapshot()
{
	model_swapcontext(&fork_snap->shared_ctxt, &private_ctxt);
	DEBUG("TAKESNAPSHOT RETURN\n");
	return snapshotid;
}

static void fork_roll_back(snapshot_id theID)
{
	DEBUG("Rollback\n");
	fork_snap->mIDToRollback = theID;
	model_swapcontext(&fork_snap->shared_ctxt, &exit_ctxt);
	fork_snap->mIDToRollback = -1;
}

#endif /* !USE_MPROTECT_SNAPSHOT */

/**
 * @brief Initializes the snapshot system
 * @param entryPoint the function that should run the program.
 */
void snapshot_system_init(unsigned int numbackingpages,
		unsigned int numsnapshots, unsigned int nummemoryregions,
		unsigned int numheappages, VoidFuncPtr entryPoint)
{
#if USE_MPROTECT_SNAPSHOT
	mprot_snapshot_init(numbackingpages, numsnapshots, nummemoryregions, numheappages, entryPoint);
#else
	fork_snapshot_init(numbackingpages, numsnapshots, nummemoryregions, numheappages, entryPoint);
#endif
}

/** Assumes that addr is page aligned. */
void snapshot_add_memory_region(void *addr, unsigned int numPages)
{
#if USE_MPROTECT_SNAPSHOT
	mprot_add_to_snapshot(addr, numPages);
#else
	/* not needed for fork-based snapshotting */
#endif
}

/** Takes a snapshot of memory.
 * @return The snapshot identifier.
 */
snapshot_id take_snapshot()
{
#if USE_MPROTECT_SNAPSHOT
	return mprot_take_snapshot();
#else
	return fork_take_snapshot();
#endif
}

/** Rolls the memory state back to the given snapshot identifier.
 *  @param theID is the snapshot identifier to rollback to.
 */
void snapshot_roll_back(snapshot_id theID)
{
#if USE_MPROTECT_SNAPSHOT
	mprot_roll_back(theID);
#else
	fork_roll_back(theID);
#endif
}
