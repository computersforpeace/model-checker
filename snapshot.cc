#include <inttypes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <map>
#include <cstring>
#include <cstdio>
#include "snapshot.h"
#include "snapshotimp.h"
#include "mymemory.h"
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/wait.h>
#include <ucontext.h>

#define FAILURE(mesg) { printf("failed in the API: %s with errno relative message: %s\n", mesg, strerror( errno ) ); exit(EXIT_FAILURE); }

#ifdef CONFIG_SSDEBUG
#define SSDEBUG		printf
#else
#define SSDEBUG(...)	do { } while (0)
#endif

/* extern declaration definition */
#if USE_MPROTECT_SNAPSHOT
struct SnapShot * snapshotrecord = NULL;
#else
struct Snapshot * sTheRecord = NULL;
#endif

#if !USE_MPROTECT_SNAPSHOT
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
#endif

/** PageAlignedAdressUpdate return a page aligned address for the
 * address being added as a side effect the numBytes are also changed.
 */
static void * PageAlignAddressUpward(void * addr) {
	return (void *)((((uintptr_t)addr)+PAGESIZE-1)&~(PAGESIZE-1));
}

#if USE_MPROTECT_SNAPSHOT

/** ReturnPageAlignedAddress returns a page aligned address for the
 * address being added as a side effect the numBytes are also changed.
 */
static void * ReturnPageAlignedAddress(void * addr) {
	return (void *)(((uintptr_t)addr)&~(PAGESIZE-1));
}

/** The initSnapShotRecord method initialized the snapshotting data
 *  structures for the mprotect based snapshot. 
 */
static void initSnapShotRecord(unsigned int numbackingpages, unsigned int numsnapshots, unsigned int nummemoryregions) {
	snapshotrecord=( struct SnapShot * )MYMALLOC(sizeof(struct SnapShot));
	snapshotrecord->regionsToSnapShot=( struct MemoryRegion * )MYMALLOC(sizeof(struct MemoryRegion)*nummemoryregions);
	snapshotrecord->backingStoreBasePtr= ( struct SnapShotPage * )MYMALLOC( sizeof( struct SnapShotPage ) * (numbackingpages + 1) );
	//Page align the backingstorepages
	snapshotrecord->backingStore=( struct SnapShotPage * )PageAlignAddressUpward(snapshotrecord->backingStoreBasePtr);
	snapshotrecord->backingRecords=( struct BackingPageRecord * )MYMALLOC(sizeof(struct BackingPageRecord)*numbackingpages);
	snapshotrecord->snapShots= ( struct SnapShotRecord * )MYMALLOC(sizeof(struct SnapShotRecord)*numsnapshots);
	snapshotrecord->lastSnapShot=0;
	snapshotrecord->lastBackingPage=0;
	snapshotrecord->lastRegion=0;
	snapshotrecord->maxRegions=nummemoryregions;
	snapshotrecord->maxBackingPages=numbackingpages;
	snapshotrecord->maxSnapShots=numsnapshots;
}

/** HandlePF is the page fault handler for mprotect based snapshotting
 * algorithm.
 */
static void HandlePF( int sig, siginfo_t *si, void * unused){
	if( si->si_code == SEGV_MAPERR ){
		printf("Real Fault at %p\n", si->si_addr);
		exit( EXIT_FAILURE );
	}
	void* addr = ReturnPageAlignedAddress(si->si_addr);

	unsigned int backingpage=snapshotrecord->lastBackingPage++; //Could run out of pages...
	if (backingpage==snapshotrecord->maxBackingPages) {
		printf("Out of backing pages at %p\n", si->si_addr);
		exit( EXIT_FAILURE );
	}

	//copy page
	memcpy(&(snapshotrecord->backingStore[backingpage]), addr, sizeof(struct SnapShotPage));
	//remember where to copy page back to
	snapshotrecord->backingRecords[backingpage].basePtrOfPage=addr;
	//set protection to read/write
	if (mprotect( addr, sizeof(struct SnapShotPage), PROT_READ | PROT_WRITE )) {
		perror("mprotect");
		// Handle error by quitting?
	}
}
#endif //nothing to handle for non snapshotting case.

void createSharedLibrary(){
#if !USE_MPROTECT_SNAPSHOT
	//step 1. create shared memory.
	if ( sTheRecord )
		return;
	int fd = shm_open( "/ModelChecker-Snapshotter", O_RDWR | O_CREAT, 0777 ); //universal permissions.
	if ( -1 == fd )
		FAILURE("shm_open");
	if ( -1 == ftruncate( fd, SHARED_MEMORY_DEFAULT + STACK_SIZE_DEFAULT ) )
		FAILURE( "ftruncate" );
	void * memMapBase = mmap( 0, SHARED_MEMORY_DEFAULT + STACK_SIZE_DEFAULT, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
	if( MAP_FAILED == memMapBase )
		FAILURE("mmap");
	sTheRecord = ( struct Snapshot * )memMapBase;
	sTheRecord->mSharedMemoryBase = (void *)((uintptr_t)memMapBase + sizeof(struct Snapshot));
	sTheRecord->mStackBase = (void *)((uintptr_t)memMapBase + SHARED_MEMORY_DEFAULT);
	sTheRecord->mStackSize = STACK_SIZE_DEFAULT;
	sTheRecord->mIDToRollback = -1;
	sTheRecord->currSnapShotID = 0;
#endif
}

/** The initSnapShotLibrary function initializes the Snapshot library.
 *  @param entryPoint the function that should run the program.
 */
void initSnapShotLibrary(unsigned int numbackingpages,
		unsigned int numsnapshots, unsigned int nummemoryregions,
		unsigned int numheappages, VoidFuncPtr entryPoint) {
#if USE_MPROTECT_SNAPSHOT
	/* Setup a stack for our signal handler....  */
	stack_t ss;
	ss.ss_sp = MYMALLOC(SIGSTACKSIZE);
	ss.ss_size = SIGSTACKSIZE;
	ss.ss_flags = 0;
	sigaltstack(&ss, NULL);

	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_RESTART | SA_ONSTACK;
	sigemptyset( &sa.sa_mask );
	sa.sa_sigaction = HandlePF;
#ifdef MAC
	if( sigaction( SIGBUS, &sa, NULL ) == -1 ){
		printf("SIGACTION CANNOT BE INSTALLED\n");
		exit(EXIT_FAILURE);
	}
#endif
	if( sigaction( SIGSEGV, &sa, NULL ) == -1 ){
		printf("SIGACTION CANNOT BE INSTALLED\n");
		exit(EXIT_FAILURE);
	}

	initSnapShotRecord(numbackingpages, numsnapshots, nummemoryregions);

	// EVIL HACK: We need to make sure that calls into the HandlePF method don't cause dynamic links
	// The problem is that we end up protecting state in the dynamic linker...
	// Solution is to call our signal handler before we start protecting stuff...

	siginfo_t si;
	memset(&si, 0, sizeof(si));
	si.si_addr=ss.ss_sp;
	HandlePF(SIGSEGV, &si, NULL);
	snapshotrecord->lastBackingPage--; //remove the fake page we copied

	basemySpace=MYMALLOC((numheappages+1)*PAGESIZE);
	void * pagealignedbase=PageAlignAddressUpward(basemySpace);
	mySpace = create_mspace_with_base(pagealignedbase,  numheappages*PAGESIZE, 1 );
	addMemoryRegionToSnapShot(pagealignedbase, numheappages);
	entryPoint();
#else

	basemySpace=system_malloc((numheappages+1)*PAGESIZE);
	void * pagealignedbase=PageAlignAddressUpward(basemySpace);
	mySpace = create_mspace_with_base(pagealignedbase,  numheappages*PAGESIZE, 1 );
	createSharedLibrary();

	//step 2 setup the stack context.
	ucontext_t newContext;
	getcontext( &newContext );
	newContext.uc_stack.ss_sp = sTheRecord->mStackBase;
	newContext.uc_stack.ss_size = STACK_SIZE_DEFAULT;
	makecontext( &newContext, entryPoint, 0 );
	/* switch to a new entryPoint context, on a new stack */
	swapcontext(&savedSnapshotContext, &newContext);

	/* switch back here when takesnapshot is called */
	pid_t forkedID = 0;
	snapshotid = sTheRecord->currSnapShotID;
	bool swapContext = false;
	while( true ){
		sTheRecord->currSnapShotID=snapshotid+1;
		forkedID = fork();
		if( 0 == forkedID ){
			ucontext_t currentContext;
			/*If the rollback bool is set{ see below }, switch to the context we need to return to during a rollback*/
			if( swapContext )
				swapcontext( &currentContext, &( sTheRecord->mContextToRollback ) );
			else{
				/*Child process which is forked as a result of takesnapshot call should switch back to the takesnapshot context*/
				swapcontext( &currentContext, &savedUserSnapshotContext );
			}
		} else {
			int status;
			int retVal;

			SSDEBUG("The process id of child is %d and the process id of this process is %d and snapshot id is %d",
			        forkedID, getpid(), snapshotid );

			do {
				retVal=waitpid( forkedID, &status, 0 );
			} while( -1 == retVal && errno == EINTR );

			if( sTheRecord->mIDToRollback != snapshotid )
				exit(EXIT_SUCCESS);
			else{
				/*This bool indicates that the current process's snapshotid is same as the id to which the rollback needs to occur*/
				swapContext = true;
			}
		}
	}

#endif
}

/** The addMemoryRegionToSnapShot function assumes that addr is page aligned. 
 */
void addMemoryRegionToSnapShot( void * addr, unsigned int numPages) {
#if USE_MPROTECT_SNAPSHOT
	unsigned int memoryregion=snapshotrecord->lastRegion++;
	if (memoryregion==snapshotrecord->maxRegions) {
		printf("Exceeded supported number of memory regions!\n");
		exit(EXIT_FAILURE);
	}

	snapshotrecord->regionsToSnapShot[ memoryregion ].basePtr=addr;
	snapshotrecord->regionsToSnapShot[ memoryregion ].sizeInPages=numPages;
#endif //NOT REQUIRED IN THE CASE OF FORK BASED SNAPSHOTS.
}

/** The takeSnapshot function takes a snapshot.
 * @return The snapshot identifier.
 */
snapshot_id takeSnapshot( ){
#if USE_MPROTECT_SNAPSHOT
	for(unsigned int region=0; region<snapshotrecord->lastRegion;region++) {
		if( mprotect(snapshotrecord->regionsToSnapShot[region].basePtr, snapshotrecord->regionsToSnapShot[region].sizeInPages*sizeof(struct SnapShotPage), PROT_READ ) == -1 ){
			perror("mprotect");
			printf("Failed to mprotect inside of takeSnapShot\n");
			exit(EXIT_FAILURE);
		}
	}
	unsigned int snapshot=snapshotrecord->lastSnapShot++;
	if (snapshot==snapshotrecord->maxSnapShots) {
		printf("Out of snapshots\n");
		exit(EXIT_FAILURE);
	}
	snapshotrecord->snapShots[snapshot].firstBackingPage=snapshotrecord->lastBackingPage;

	return snapshot;
#else
	swapcontext( &savedUserSnapshotContext, &savedSnapshotContext );
	return snapshotid;
#endif
}

/** The rollBack function rollback to the given snapshot identifier.
 *  @param theID is the snapshot identifier to rollback to.
 */
void rollBack( snapshot_id theID ){
#if USE_MPROTECT_SNAPSHOT
	std::map< void *, bool, std::less< void * >, MyAlloc< std::pair< const void *, bool > > > duplicateMap;
	for(unsigned int region=0; region<snapshotrecord->lastRegion;region++) {
		if( mprotect(snapshotrecord->regionsToSnapShot[region].basePtr, snapshotrecord->regionsToSnapShot[region].sizeInPages*sizeof(struct SnapShotPage), PROT_READ | PROT_WRITE ) == -1 ){
			perror("mprotect");
			printf("Failed to mprotect inside of takeSnapShot\n");
			exit(EXIT_FAILURE);
		}
	}
	for(unsigned int page=snapshotrecord->snapShots[theID].firstBackingPage; page<snapshotrecord->lastBackingPage; page++) {
		bool oldVal = false;
		if( duplicateMap.find( snapshotrecord->backingRecords[page].basePtrOfPage ) != duplicateMap.end() ){
			oldVal = true;
		}
		else{
			duplicateMap[ snapshotrecord->backingRecords[page].basePtrOfPage ] = true;
		}
		if(  !oldVal ){
			memcpy(snapshotrecord->backingRecords[page].basePtrOfPage, &snapshotrecord->backingStore[page], sizeof(struct SnapShotPage));
		}
	}
	snapshotrecord->lastSnapShot=theID;
	snapshotrecord->lastBackingPage=snapshotrecord->snapShots[theID].firstBackingPage;
	takeSnapshot(); //Make sure current snapshot is still good...All later ones are cleared
#else
	sTheRecord->mIDToRollback = theID;
	int sTemp = 0;
	getcontext( &sTheRecord->mContextToRollback );
	/*
	* This is used to quit the process on rollback, so that
	* the process which needs to rollback can quit allowing the process whose snapshotid matches the rollbackid to switch to 
	* this context and continue....
	*/
	if( !sTemp ){
		sTemp = 1;
		SSDEBUG("Invoked rollback");
		exit(EXIT_SUCCESS);
	}
    /*
	* This fix obviates the need for a finalize call. hence less dependences for model-checker....
    *
    */
	sTheRecord->mIDToRollback = -1;
#endif
}

