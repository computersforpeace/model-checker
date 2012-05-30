#ifndef _SNAPSHOT_H
#define _SNAPSHOT_H
#define PAGESIZE 4096

/* If USE_MPROTECT_SNAPSHOT=1, then snapshot by using mmap() and mprotect()
   If USE_MPROTECT_SNAPSHOT=0, then snapshot by using fork() */
#define USE_MPROTECT_SNAPSHOT 1

/* Size of signal stack */
#define SIGSTACKSIZE 16384


typedef unsigned int snapshot_id;
typedef void (*MyFuncPtr)();
void initSnapShotLibrary(unsigned int numbackingpages, unsigned int numsnapshots, unsigned int nummemoryregions, unsigned int numheappages, MyFuncPtr entryPoint);

void addMemoryRegionToSnapShot( void * ptr, unsigned int numPages );

snapshot_id takeSnapshot( );

void rollBack( snapshot_id theSnapShot );

void finalize();

#ifdef __cplusplus
extern "C" {
#endif
void createSharedLibrary();
#ifdef __cplusplus
};  /* end of extern "C" */
#endif
#endif
