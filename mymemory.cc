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

/** Non-snapshotting calloc for our use. */
void *model_calloc(size_t count, size_t size) {
#if USE_MPROTECT_SNAPSHOT
	static void *(*callocp)(size_t count, size_t size)=NULL;
	char *error;
	void *ptr;

	/* get address of libc malloc */
	if (!callocp) {
		callocp = ( void * ( * )( size_t, size_t ) )dlsym(RTLD_NEXT, "calloc");
		if ((error = dlerror()) != NULL) {
			fputs(error, stderr);
			exit(EXIT_FAILURE);
		}
	}
	ptr = callocp(count, size);
	return ptr;
#else
	if( !snapshotrecord) {
		createSharedMemory();
	}
	if( NULL == sStaticSpace )
		sStaticSpace = create_mspace_with_base( ( void * )( snapshotrecord->mSharedMemoryBase ), SHARED_MEMORY_DEFAULT -sizeof( struct SnapShot ), 1 );
	return mspace_calloc( sStaticSpace, count, size );
#endif
}

/** Non-snapshotting malloc for our use. */
void *model_malloc(size_t size) {
#if USE_MPROTECT_SNAPSHOT
	static void *(*mallocp)(size_t size)=NULL;
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
	if( !snapshotrecord) {
		createSharedMemory();
	}
	if( NULL == sStaticSpace )
		sStaticSpace = create_mspace_with_base( ( void * )( snapshotrecord->mSharedMemoryBase ), SHARED_MEMORY_DEFAULT -sizeof( struct SnapShot ), 1 );
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

/** Bootstrap allocation.  Problem is that the dynamic linker calls
 *  require calloc to work and calloc requires the dynamic linker to
 *	work.  */

#define BOOTSTRAPBYTES 4096
char bootstrapmemory[BOOTSTRAPBYTES];
size_t offset=0;

void * HandleEarlyAllocationRequest( size_t sz ){
	/*Align to 8 byte boundary*/
	sz=(sz+7)&~7;

	if (sz > (BOOTSTRAPBYTES-offset)) {
		printf("OUT OF BOOTSTRAP MEMORY\n");
		exit(EXIT_FAILURE);
	}

	void * pointer= (void *) & bootstrapmemory[offset];
	offset+=sz;
	return pointer;
}

/** Check whether this is bootstrapped memory that we should not
		free. */

bool DontFree( void * ptr ){
	return (ptr>=(&bootstrapmemory[0])&&ptr<(&bootstrapmemory[BOOTSTRAPBYTES]));
}

/** Snapshotting malloc implementation for user programs. */
void *malloc( size_t size ) {
	if (mySpace)
		return mspace_malloc( mySpace, size );
	else
		return HandleEarlyAllocationRequest( size );
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
	if (mySpace)
		return mspace_calloc( mySpace, num, size );
	else {
		void *tmp=HandleEarlyAllocationRequest( size * num );
		std::memset( tmp, 0, size * num );
		return tmp;
	}
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
