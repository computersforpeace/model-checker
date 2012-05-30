#ifndef _MY_MEMORY_H
#define _MY_MEMORY_H
#include <stdlib.h>
#include <limits>

#define MEMALLOC \
	void * operator new(size_t size) { \
		return MYMALLOC(size);\
	}\
	void operator delete(void *p, size_t size) { \
		MYFREE( p ); \
	}\
	void * operator new[](size_t size) { \
		return MYMALLOC(size);\
	}\
	void operator delete[](void *p, size_t size) {\
		MYFREE(p);\
	}

/* Empty define; represents opposite of MEMALLOC */
#define SNAPSHOTALLOC

void *MYMALLOC(size_t size);
void MYFREE(void *ptr);

void system_free( void * ptr );
void *system_malloc( size_t size );
/*
The following code example is taken from the book
The C++ Standard Library - A Tutorial and Reference
by Nicolai M. Josuttis, Addison-Wesley, 1999
© Copyright Nicolai M. Josuttis 1999
Permission to copy, use, modify, sell and distribute this software
is granted provided this copyright notice appears in all copies.
This software is provided "as is" without express or implied
warranty, and with no claim as to its suitability for any purpose.
*/
template <class T>
   class MyAlloc {
     public:
       // type definitions
       typedef T        value_type;
       typedef T*       pointer;
       typedef const T* const_pointer;
       typedef T&       reference;
       typedef const T& const_reference;
       typedef size_t   size_type;
       typedef size_t difference_type;

       // rebind allocator to type U
       template <class U>
       struct rebind {
           typedef MyAlloc<U> other;
       };

       // return address of values
       pointer address (reference value) const {
           return &value;
       }
       const_pointer address (const_reference value) const {
           return &value;
       }

       /* constructors and destructor
        * - nothing to do because the allocator has no state
        */
       MyAlloc() throw() {
       }
       MyAlloc(const MyAlloc&) throw() {
       }
       template <class U>
         MyAlloc (const MyAlloc<U>&) throw() {
       }
       ~MyAlloc() throw() {
       }

       // return maximum number of elements that can be allocated
       size_type max_size () const throw() {
           return std::numeric_limits<size_t>::max() / sizeof(T);
       }

       // allocate but don't initialize num elements of type T
       pointer allocate (size_type num, const void* = 0) {
           pointer p = ( pointer )MYMALLOC( num * sizeof( T ) );
           return p;
       }

       // initialize elements of allocated storage p with value value
       void construct (pointer p, const T& value) {
           // initialize memory with placement new
           new((void*)p)T(value);
       }

       // destroy elements of initialized storage p
       void destroy (pointer p) {
           // destroy objects by calling their destructor
           p->~T();
       }

       // deallocate storage p of deleted elements
       void deallocate (pointer p, size_type num) {
           MYFREE((void*)p);
       }
   };

// return that all specializations of this allocator are interchangeable
 template <class T1, class T2>
 bool operator== (const MyAlloc<T1>&,
                  const MyAlloc<T2>&) throw() {
     return true;
 }
 template <class T1, class T2>
 bool operator!= (const MyAlloc<T1>&,
                  const MyAlloc<T2>&) throw() {
     return false;
 }

#ifdef __cplusplus
extern "C" {
#endif
typedef void * mspace;
extern void* mspace_malloc(mspace msp, size_t bytes);
extern void mspace_free(mspace msp, void* mem);
extern void* mspace_realloc(mspace msp, void* mem, size_t newsize);
extern void* mspace_calloc(mspace msp, size_t n_elements, size_t elem_size);
extern mspace create_mspace_with_base(void* base, size_t capacity, int locked);
extern mspace create_mspace(size_t capacity, int locked);
extern mspace mySpace;
extern void * basemySpace;
#ifdef __cplusplus
};  /* end of extern "C" */
#endif

#endif /* _MY_MEMORY_H */
