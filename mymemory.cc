#include "mymemory.h"
#include "snapshot.h"
#include "snapshotimp.h"
#include <stdio.h>
#include <dlfcn.h>
#define MSPACE_SIZE ( 1 << 20 )
#if !USE_CHECKPOINTING
static mspace sStaticSpace = NULL;
#endif

void *MYMALLOC(size_t size) {
#if USE_CHECKPOINTING
  static void *(*mallocp)(size_t size);
  char *error;
  void *ptr;

  /* get address of libc malloc */
  if (!mallocp) {
    mallocp = ( void * ( * )( size_t ) )dlsym(RTLD_NEXT, "malloc");
    if ((error = dlerror()) != NULL) {
      fputs(error, stderr);
      exit(1);
    }
  }
  ptr = mallocp(size);     
  return ptr;
#else
  if( !sTheRecord ){
    createSharedLibrary();
  }
  if( NULL == sStaticSpace )
    sStaticSpace = create_mspace_with_base( ( void * )( sTheRecord->mSharedMemoryBase ), SHARED_MEMORY_DEFAULT -sizeof( struct Snapshot_t ), 1 );
  return mspace_malloc( sStaticSpace, size );
#endif
}

void MYFREE(void *ptr) {
#if USE_CHECKPOINTING
  static void (*freep)(void *);
  char *error;

  /* get address of libc free */
  if (!freep) {
    freep = ( void  ( * )( void * ) )dlsym(RTLD_NEXT, "free");
    if ((error = dlerror()) != NULL) {
      fputs(error, stderr);
      exit(1);
    }
  }
  freep(ptr);
#else
  mspace_free( sStaticSpace, ptr );
#endif
}
static mspace mySpace = NULL;
void *malloc( size_t size ) {
  if( NULL == mySpace ){
    //void * mem = MYMALLOC( MSPACE_SIZE );
    mySpace = create_mspace( MSPACE_SIZE, 1 );
    AddUserHeapToSnapshot();
  }
  return mspace_malloc( mySpace, size );
}

void free( void * ptr ){
  mspace_free( mySpace, ptr );
}

void AddUserHeapToSnapshot(){
  static bool alreadySnapshotted = false;
  if( alreadySnapshotted ) return;
  addMemoryRegionToSnapShot( mySpace, MSPACE_SIZE / PAGESIZE );
}


void * operator new(size_t size) throw(std::bad_alloc) {
  return MYMALLOC(size);
}

void operator delete(void *p) throw() {
  MYFREE(p);
}
