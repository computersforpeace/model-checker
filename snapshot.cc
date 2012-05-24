#include <inttypes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <map>
#include <set>
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
#include <sys/time.h>
//extern declaration definition
#define FAILURE(mesg) { printf("failed in the API: %s with errno relative message: %s\n", mesg, strerror( errno ) ); exit( -1 ); }
#if USE_CHECKPOINTING
struct SnapShot * snapshotrecord = NULL;
struct Snapshot_t * sTheRecord = NULL;
#else
struct Snapshot_t * sTheRecord = NULL;
#endif
void BeginOperation( struct timeval * theStartTime ){
#if 1
	gettimeofday( theStartTime, NULL );
#endif
}
#if SSDEBUG
struct timeval *starttime = NULL;
#endif
void DumpIntoLog( const char * filename, const char * message ){
#if SSDEBUG
   static pid_t thePID = getpid();
   char newFn[ 1024 ] ={ 0 };
   sprintf( newFn,"%s-%d.txt", filename, thePID );
   FILE * myFile = fopen( newFn, "w+" );
   struct timeval theEndTime;
   BeginOperation( &theEndTime );
   double elapsed = ( theEndTime.tv_sec - starttime->tv_sec ) + ( theEndTime.tv_usec - starttime->tv_usec ) / 1000000.0;
   fprintf( myFile, "The timestamp %f:--> the message %s: the process id %d\n", elapsed, message, thePID );
   fflush( myFile );
   fclose( myFile );
   myFile = NULL;
#endif
}
#if !USE_CHECKPOINTING
static ucontext_t savedSnapshotContext;
static ucontext_t savedUserSnapshotContext;
static int snapshotid = 0;
#endif
/* Initialize snapshot data structure */
#if USE_CHECKPOINTING
void initSnapShotRecord(unsigned int numbackingpages, unsigned int numsnapshots, unsigned int nummemoryregions) {
  snapshotrecord=( struct SnapShot * )MYMALLOC(sizeof(struct SnapShot));
  snapshotrecord->regionsToSnapShot=( struct MemoryRegion * )MYMALLOC(sizeof(struct MemoryRegion)*nummemoryregions);
  snapshotrecord->backingStoreBasePtr= ( struct SnapShotPage * )MYMALLOC( sizeof( struct SnapShotPage ) * (numbackingpages + 1) );
  //Page align the backingstorepages
  snapshotrecord->backingStore=( struct SnapShotPage * )ReturnPageAlignedAddress((void*) ((uintptr_t)(snapshotrecord->backingStoreBasePtr)+sizeof(struct SnapShotPage)-1));
  snapshotrecord->backingRecords=( struct BackingPageRecord * )MYMALLOC(sizeof(struct BackingPageRecord)*numbackingpages);
  snapshotrecord->snapShots= ( struct SnapShotRecord * )MYMALLOC(sizeof(struct SnapShotRecord)*numsnapshots);
  snapshotrecord->lastSnapShot=0;
  snapshotrecord->lastBackingPage=0;
  snapshotrecord->lastRegion=0;
  snapshotrecord->maxRegions=nummemoryregions;
  snapshotrecord->maxBackingPages=numbackingpages;
  snapshotrecord->maxSnapShots=numsnapshots;
}
#endif //nothing to initialize for the fork based snapshotting.

void HandlePF( int sig, siginfo_t *si, void * unused){
#if USE_CHECKPOINTING
  if( si->si_code == SEGV_MAPERR ){
    printf("Real Fault at %llx\n", ( long long )si->si_addr);
    exit( EXIT_FAILURE );	
  }
  void* addr = ReturnPageAlignedAddress(si->si_addr);
  unsigned int backingpage=snapshotrecord->lastBackingPage++; //Could run out of pages...
  if (backingpage==snapshotrecord->maxBackingPages) {
    printf("Out of backing pages at %llx\n", ( long long )si->si_addr);
    exit( EXIT_FAILURE );	
  }

  //copy page
  memcpy(&(snapshotrecord->backingStore[backingpage]), addr, sizeof(struct SnapShotPage));
  //remember where to copy page back to
  snapshotrecord->backingRecords[backingpage].basePtrOfPage=addr;
  //set protection to read/write
  mprotect( addr, sizeof(struct SnapShotPage), PROT_READ | PROT_WRITE );  
#endif //nothing to handle for non snapshotting case.
}

//Return a page aligned address for the address being added
//as a side effect the numBytes are also changed.
void * ReturnPageAlignedAddress(void * addr) {
  return (void *)(((uintptr_t)addr)&~(PAGESIZE-1));
}
#ifdef __cplusplus
extern "C" {
#endif
void createSharedLibrary(){
#if !USE_CHECKPOINTING
	  //step 1. create shared memory.
  if( sTheRecord ) return;
  int fd = shm_open( "/ModelChecker-Snapshotter", O_RDWR | O_CREAT, 0777 ); //universal permissions.
  if( -1 == fd ) FAILURE("shm_open");
  if( -1 == ftruncate( fd, ( size_t )SHARED_MEMORY_DEFAULT + ( size_t )STACK_SIZE_DEFAULT ) ) FAILURE( "ftruncate" );
  char * memMapBase = ( char * ) mmap( 0, ( size_t )SHARED_MEMORY_DEFAULT + ( size_t )STACK_SIZE_DEFAULT, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
  if( MAP_FAILED == memMapBase ) FAILURE("mmap");
  sTheRecord = ( struct Snapshot_t * )memMapBase;
  sTheRecord->mSharedMemoryBase = memMapBase + sizeof( struct Snapshot_t );
  sTheRecord->mStackBase = ( char * )memMapBase + ( size_t )SHARED_MEMORY_DEFAULT;
  sTheRecord->mStackSize = STACK_SIZE_DEFAULT;
  sTheRecord->mIDToRollback = -1;
  sTheRecord->currSnapShotID = 0;
#endif
}
#ifdef __cplusplus
}
#endif
void initSnapShotLibrary(unsigned int numbackingpages, unsigned int numsnapshots, unsigned int nummemoryregions, unsigned int numheappages, MyFuncPtr entryPoint){
#if USE_CHECKPOINTING
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_RESTART;
  sigemptyset( &sa.sa_mask );
  sa.sa_sigaction = HandlePF;
  if( sigaction( SIGSEGV, &sa, NULL ) == -1 ){
    printf("SIGACTION CANNOT BE INSTALLED\n");
    exit(-1);
  }
  mySpace = create_mspace( numheappages*PAGESIZE, 1 );
  initSnapShotRecord(numbackingpages, numsnapshots, nummemoryregions);
  addMemoryRegionToSnapShot(mySpace, numheappages);
  entryPoint();
#else
  //add a signal to indicate that the process is going to terminate.
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO | SA_NODEFER | SA_RESTART;
  sigemptyset( &sa.sa_mask );
  sa.sa_sigaction = HandlePF;
  if( sigaction( SIGUSR1, &sa, NULL ) == -1 ){
    printf("SIGACTION CANNOT BE INSTALLED\n");
    exit(-1);
  }
  createSharedLibrary();
 #if SSDEBUG
  starttime = &(sTheRecord->startTimeGlobal);
  gettimeofday( starttime, NULL );
#endif
  //step 2 setup the stack context.
 
  int alreadySwapped = 0;
  getcontext( &savedSnapshotContext );
  if( !alreadySwapped ){
    alreadySwapped = 1;
    ucontext_t currentContext, swappedContext, newContext;
    getcontext( &newContext );
    newContext.uc_stack.ss_sp = sTheRecord->mStackBase;
    newContext.uc_stack.ss_size = STACK_SIZE_DEFAULT;
    newContext.uc_link = &currentContext;
    makecontext( &newContext, entryPoint, 0 );
    swapcontext( &swappedContext, &newContext );
  }
  
   //add the code to take a snapshot here...
	//to return to user process, do a second swapcontext...
  pid_t forkedID = 0;
  snapshotid = sTheRecord->currSnapShotID;
  bool swapContext = false;
  while( !sTheRecord->mbFinalize ){
    sTheRecord->currSnapShotID=snapshotid+1;
    forkedID = fork();
    if( 0 == forkedID ){ 
      ucontext_t currentContext;
#if 0
      int dbg = 0;
      while( !dbg );
#endif
      if( swapContext )
        swapcontext( &currentContext, &( sTheRecord->mContextToRollback ) );
			else{
				swapcontext( &currentContext, &savedUserSnapshotContext );      
			}
    } else {
      int status;
      int retVal;
#if SSDEBUG
      char mesg[ 1024 ] = { 0 };
      sprintf( mesg, "The process id of child is %d and the process id of this process is %d and snapshot id is %d", forkedID, getpid(), snapshotid );
      DumpIntoLog( "ModelSnapshot", mesg );
#endif
      do { 
				retVal=waitpid( forkedID, &status, 0 );
      } while( -1 == retVal && errno == EINTR );

      if( sTheRecord->mIDToRollback != snapshotid )
			  exit(0);
      else{
         swapContext = true;
      }
    }
  }
  
#endif
}
/* This function assumes that addr is page aligned */
void addMemoryRegionToSnapShot( void * addr, unsigned int numPages) {
#if USE_CHECKPOINTING
  unsigned int memoryregion=snapshotrecord->lastRegion++;
  if (memoryregion==snapshotrecord->maxRegions) {
    printf("Exceeded supported number of memory regions!\n");
    exit(-1);
  }
  
  snapshotrecord->regionsToSnapShot[ memoryregion ].basePtr=addr;
  snapshotrecord->regionsToSnapShot[ memoryregion ].sizeInPages=numPages;
#endif //NOT REQUIRED IN THE CASE OF FORK BASED SNAPSHOTS.
}
//take snapshot
snapshot_id takeSnapshot( ){
#if USE_CHECKPOINTING
  for(unsigned int region=0; region<snapshotrecord->lastRegion;region++) {
    if( mprotect(snapshotrecord->regionsToSnapShot[region].basePtr, snapshotrecord->regionsToSnapShot[region].sizeInPages*sizeof(struct SnapShotPage), PROT_READ ) == -1 ){
      printf("Failed to mprotect inside of takeSnapShot\n");
      exit(-1);
    }		
  }
  unsigned int snapshot=snapshotrecord->lastSnapShot++;
  if (snapshot==snapshotrecord->maxSnapShots) {
    printf("Out of snapshots\n");
    exit(-1);
  }
  snapshotrecord->snapShots[snapshot].firstBackingPage=snapshotrecord->lastBackingPage;
  
  return snapshot;
#else
  swapcontext( &savedUserSnapshotContext, &savedSnapshotContext );
  return snapshotid;
#endif
}
void rollBack( snapshot_id theID ){
#if USE_CHECKPOINTING
  std::map< void *, bool, std::less< void * >, MyAlloc< std::pair< const void *, bool > > > duplicateMap;
  for(unsigned int region=0; region<snapshotrecord->lastRegion;region++) {
  if( mprotect(snapshotrecord->regionsToSnapShot[region].basePtr, snapshotrecord->regionsToSnapShot[region].sizeInPages*sizeof(struct SnapShotPage), PROT_READ | PROT_WRITE ) == -1 ){
      printf("Failed to mprotect inside of takeSnapShot\n");
      exit(-1);
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
  if( !sTemp ){
    sTemp = 1;
#if SSDEBUG
  	DumpIntoLog( "ModelSnapshot", "Invoked rollback" ); 
#endif
  	exit( 0 );
  }
#endif
}

void finalize(){
#if !USE_CHECKPOINTING
  sTheRecord->mbFinalize = true;
#endif
}

