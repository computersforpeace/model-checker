#include "mymemory.h"
#include "snapshot.h"
#include "snapshotimp.h"
#include <stdio.h>
#include <dlfcn.h>
#if !USE_MPROTECT_SNAPSHOT
static mspace sStaticSpace = NULL;
#endif

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
		sStaticSpace = create_mspace_with_base( ( void * )( sTheRecord->mSharedMemoryBase ), SHARED_MEMORY_DEFAULT -sizeof( struct Snapshot_t ), 1 );
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
mspace mySpace = NULL;
void * basemySpace = NULL;

void *malloc( size_t size ) {
	return mspace_malloc( mySpace, size );
}

void free( void * ptr ){
	mspace_free( mySpace, ptr );
}

void *realloc( void *ptr, size_t size ){
	return mspace_realloc( mySpace, ptr, size );
}

void * operator new(size_t size) throw(std::bad_alloc) {
	return malloc(size);
}

void operator delete(void *p) throw() {
	free(p);
}

void * operator new[](size_t size) throw(std::bad_alloc) {
	return malloc(size);
}

void operator delete[](void *p, size_t size) {
	free(p);
}
