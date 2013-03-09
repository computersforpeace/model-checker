/** @file mymemory.h
 *  @brief Memory allocation functions.
 */

#ifndef _MY_MEMORY_H
#define _MY_MEMORY_H
#include <limits>
#include <stddef.h>

#include "config.h"

/** MEMALLOC declares the allocators for a class to allocate
 *	memory in the non-snapshotting heap. */
#define MEMALLOC \
	void * operator new(size_t size) { \
		return model_malloc(size); \
	} \
	void operator delete(void *p, size_t size) { \
		model_free(p); \
	} \
	void * operator new[](size_t size) { \
		return model_malloc(size); \
	} \
	void operator delete[](void *p, size_t size) { \
		model_free(p); \
	} \
	void * operator new(size_t size, void *p) { /* placement new */ \
		return p; \
	}

/** SNAPSHOTALLOC declares the allocators for a class to allocate
 *	memory in the snapshotting heap. */
#define SNAPSHOTALLOC \
	void * operator new(size_t size) { \
		return snapshot_malloc(size); \
	} \
	void operator delete(void *p, size_t size) { \
		snapshot_free(p); \
	} \
	void * operator new[](size_t size) { \
		return snapshot_malloc(size); \
	} \
	void operator delete[](void *p, size_t size) { \
		snapshot_free(p); \
	} \
	void * operator new(size_t size, void *p) { /* placement new */ \
		return p; \
	}

void *model_malloc(size_t size);
void *model_calloc(size_t count, size_t size);
void model_free(void *ptr);

void * snapshot_malloc(size_t size);
void * snapshot_calloc(size_t count, size_t size);
void * snapshot_realloc(void *ptr, size_t size);
void snapshot_free(void *ptr);

void * Thread_malloc(size_t size);
void Thread_free(void *ptr);

/** @brief Provides a non-snapshotting allocator for use in STL classes.
 *
 * The code was adapted from a code example from the book The C++
 * Standard Library - A Tutorial and Reference by Nicolai M. Josuttis,
 * Addison-Wesley, 1999 © Copyright Nicolai M. Josuttis 1999
 * Permission to copy, use, modify, sell and distribute this software
 * is granted provided this copyright notice appears in all copies.
 * This software is provided "as is" without express or implied
 * warranty, and with no claim as to its suitability for any purpose.
 */
template <class T>
class ModelAlloc {
 public:
	// type definitions
	typedef T        value_type;
	typedef T*       pointer;
	typedef const T* const_pointer;
	typedef T&       reference;
	typedef const T& const_reference;
	typedef size_t   size_type;
	typedef size_t   difference_type;

	// rebind allocator to type U
	template <class U>
	struct rebind {
		typedef ModelAlloc<U> other;
	};

	// return address of values
	pointer address(reference value) const {
		return &value;
	}
	const_pointer address(const_reference value) const {
		return &value;
	}

	/* constructors and destructor
	 * - nothing to do because the allocator has no state
	 */
	ModelAlloc() throw() {
	}
	ModelAlloc(const ModelAlloc&) throw() {
	}
	template <class U>
	ModelAlloc(const ModelAlloc<U>&) throw() {
	}
	~ModelAlloc() throw() {
	}

	// return maximum number of elements that can be allocated
	size_type max_size() const throw() {
		return std::numeric_limits<size_t>::max() / sizeof(T);
	}

	// allocate but don't initialize num elements of type T
	pointer allocate(size_type num, const void * = 0) {
		pointer p = (pointer)model_malloc(num * sizeof(T));
		return p;
	}

	// initialize elements of allocated storage p with value value
	void construct(pointer p, const T& value) {
		// initialize memory with placement new
		new((void*)p)T(value);
	}

	// destroy elements of initialized storage p
	void destroy(pointer p) {
		// destroy objects by calling their destructor
		p->~T();
	}

	// deallocate storage p of deleted elements
	void deallocate(pointer p, size_type num) {
		model_free((void*)p);
	}
};

/** Return that all specializations of this allocator are interchangeable. */
template <class T1, class T2>
bool operator ==(const ModelAlloc<T1>&,
		const ModelAlloc<T2>&) throw() {
	return true;
}

/** Return that all specializations of this allocator are interchangeable. */
template <class T1, class T2>
bool operator!= (const ModelAlloc<T1>&,
		const ModelAlloc<T2>&) throw() {
	return false;
}

/** @brief Provides a snapshotting allocator for use in STL classes.
 *
 * The code was adapted from a code example from the book The C++
 * Standard Library - A Tutorial and Reference by Nicolai M. Josuttis,
 * Addison-Wesley, 1999 © Copyright Nicolai M. Josuttis 1999
 * Permission to copy, use, modify, sell and distribute this software
 * is granted provided this copyright notice appears in all copies.
 * This software is provided "as is" without express or implied
 * warranty, and with no claim as to its suitability for any purpose.
 */
template <class T>
class SnapshotAlloc {
 public:
	// type definitions
	typedef T        value_type;
	typedef T*       pointer;
	typedef const T* const_pointer;
	typedef T&       reference;
	typedef const T& const_reference;
	typedef size_t   size_type;
	typedef size_t   difference_type;

	// rebind allocator to type U
	template <class U>
	struct rebind {
		typedef SnapshotAlloc<U> other;
	};

	// return address of values
	pointer address(reference value) const {
		return &value;
	}
	const_pointer address(const_reference value) const {
		return &value;
	}

	/* constructors and destructor
	 * - nothing to do because the allocator has no state
	 */
	SnapshotAlloc() throw() {
	}
	SnapshotAlloc(const SnapshotAlloc&) throw() {
	}
	template <class U>
	SnapshotAlloc(const SnapshotAlloc<U>&) throw() {
	}
	~SnapshotAlloc() throw() {
	}

	// return maximum number of elements that can be allocated
	size_type max_size() const throw() {
		return std::numeric_limits<size_t>::max() / sizeof(T);
	}

	// allocate but don't initialize num elements of type T
	pointer allocate(size_type num, const void * = 0) {
		pointer p = (pointer)snapshot_malloc(num * sizeof(T));
		return p;
	}

	// initialize elements of allocated storage p with value value
	void construct(pointer p, const T& value) {
		// initialize memory with placement new
		new((void*)p)T(value);
	}

	// destroy elements of initialized storage p
	void destroy(pointer p) {
		// destroy objects by calling their destructor
		p->~T();
	}

	// deallocate storage p of deleted elements
	void deallocate(pointer p, size_type num) {
		snapshot_free((void*)p);
	}
};

/** Return that all specializations of this allocator are interchangeable. */
template <class T1, class T2>
bool operator ==(const SnapshotAlloc<T1>&,
		const SnapshotAlloc<T2>&) throw() {
	return true;
}

/** Return that all specializations of this allocator are interchangeable. */
template <class T1, class T2>
bool operator!= (const SnapshotAlloc<T1>&,
		const SnapshotAlloc<T2>&) throw() {
	return false;
}

#ifdef __cplusplus
extern "C" {
#endif
	typedef void * mspace;
	extern void * mspace_malloc(mspace msp, size_t bytes);
	extern void mspace_free(mspace msp, void* mem);
	extern void * mspace_realloc(mspace msp, void* mem, size_t newsize);
	extern void * mspace_calloc(mspace msp, size_t n_elements, size_t elem_size);
	extern mspace create_mspace_with_base(void* base, size_t capacity, int locked);
	extern mspace create_mspace(size_t capacity, int locked);

#if USE_MPROTECT_SNAPSHOT
	extern mspace user_snapshot_space;
#endif

	extern mspace model_snapshot_space;

#ifdef __cplusplus
};  /* end of extern "C" */
#endif

#endif /* _MY_MEMORY_H */
