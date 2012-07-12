#include "mymemory.h"
#include "snapshot.h"
#include "snapshotimp.h"
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstring>
#define REQUESTS_BEFORE_ALLOC 1024
size_t allocatedReqs[ REQUESTS_BEFORE_ALLOC ] = { 0 };
int nextRequest = 0;
int howManyFreed = 0;
#if !USE_MPROTECT_SNAPSHOT
static mspace sStaticSpace = NULL;
#endif

/** Non-snapshotting malloc for our use. */
void *MYMALLOC(size_t size) {
#if USE_MPROTECT_SNAPSHOT
	static void *(*mallocp)(size_t size);
	char *error;
	void *ptr;

	/* get address of libc malloc */
	if (!mallocp) {
		mallocp = ( void * ( * )( size_t ) )dlsym(RTLD_NEXT, "malloc");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	ptr = mallocp(size);
	return ptr;
#else
	if( !sTheRecord ){
		createSharedLibrary();
	}
	if( NULL == sStaticSpace )
		sStaticSpace = create_mspace_with_base( ( void * )( sTheRecord->mSharedMemoryBase ), SHARED_MEMORY_DEFAULT -sizeof( struct Snapshot ), 1 );
	return mspace_malloc( sStaticSpace, size );
#endif
}

void *system_malloc( size_t size ){
	static void *(*mallocp)(size_t size);
	char *error;
	void *ptr;

	/* get address of libc malloc */
	if (!mallocp) {
		mallocp = ( void * ( * )( size_t ) )dlsym(RTLD_NEXT, "malloc");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	ptr = mallocp(size);
	return ptr;
}

void system_free( void * ptr ){
	static void (*freep)(void *);
	char *error;

	/* get address of libc free */
	if (!freep) {
		freep = ( void  ( * )( void * ) )dlsym(RTLD_NEXT, "free");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	freep(ptr);
}

/** Non-snapshotting free for our use. */
void MYFREE(void *ptr) {
#if USE_MPROTECT_SNAPSHOT
	static void (*freep)(void *);
	char *error;

	/* get address of libc free */
	if (!freep) {
		freep = ( void  ( * )( void * ) )dlsym(RTLD_NEXT, "free");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	freep(ptr);
#else
	mspace_free( sStaticSpace, ptr );
#endif
}


/** This global references the mspace for the snapshotting heap */
mspace mySpace = NULL;

/** This global references the unaligned memory address that was malloced for the snapshotting heap */
void * basemySpace = NULL;

/** Adding the fix for not able to allocate through a reimplemented calloc at the beginning before instantiating our allocator
A bit circumspect about adding an sbrk. linux docs say to avoid using it... */
void * HandleEarlyAllocationRequest( size_t sz ){
	if( 0 == mySpace ){
		void * returnAddress = sbrk( sz );
		if( nextRequest >= REQUESTS_BEFORE_ALLOC ){
			exit( EXIT_FAILURE );
		}
		allocatedReqs[ nextRequest++ ] = ( size_t )returnAddress;
		return returnAddress;
	}
	return NULL;
}

/** The fact that I am not expecting more than a handful requests is implicit in my not using a binary search here*/
bool DontFree( void * ptr ){
	if( howManyFreed == nextRequest ) return false; //a minor optimization to reduce the number of instructions executed on each free call....
	if( NULL == ptr ) return true;
	for( int i =  nextRequest - 1; i >= 0; --i ){
		if( allocatedReqs[ i ] ==  ( size_t )ptr ) {
			++howManyFreed;
			return true;
		}
	}
	return false;
}

/** Snapshotting malloc implementation for user programs. */
void *malloc( size_t size ) {
	void * earlyReq = HandleEarlyAllocationRequest( size );
	if( earlyReq ) return earlyReq;
	return mspace_malloc( mySpace, size );
}

/** Snapshotting free implementation for user programs. */
void free( void * ptr ){
	if( DontFree( ptr ) ) return;
	mspace_free( mySpace, ptr );
}

/** Snapshotting realloc implementation for user programs. */
void *realloc( void *ptr, size_t size ){
	return mspace_realloc( mySpace, ptr, size );
}

/** Snapshotting calloc implementation for user programs. */
void * calloc( size_t num, size_t size ){
	void * earlyReq = HandleEarlyAllocationRequest( size * num );
	if( earlyReq ) {
		std::memset( earlyReq, 0, size * num );
		return earlyReq;
	}
	return mspace_calloc( mySpace, num, size );
}

/** Snapshotting new operator for user programs. */
void * operator new(size_t size) throw(std::bad_alloc) {
	return malloc(size);
}

/** Snapshotting delete operator for user programs. */
void operator delete(void *p) throw() {
	free(p);
}

/** Snapshotting new[] operator for user programs. */
void * operator new[](size_t size) throw(std::bad_alloc) {
	return malloc(size);
}

/** Snapshotting delete[] operator for user programs. */
void operator delete[](void *p, size_t size) {
	free(p);
}
