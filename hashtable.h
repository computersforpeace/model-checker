/** @file hashtable.h
 *  @brief Hashtable.  Standard chained bucket variety.
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdlib.h>
#include <stdio.h>

/**
 * Hashtable linked node class, for chained storage of hash table conflicts. By
 * default it is snapshotting, but you can pass in your own allocation
 * functions.
 *
 * @tparam _Key    Type name for the key
 * @tparam _Val    Type name for the values to be stored
 * @tparam _malloc Provide your own 'malloc' for the table, or default to
 *                 snapshotting.
 * @tparam _calloc Provide your own 'calloc' for the table, or default to
 *                 snapshotting.
 * @tparam _free   Provide your own 'free' for the table, or default to
 *                 snapshotting.
 */
template<typename _Key, typename _Val, void * (* _malloc)(size_t), void * (* _calloc)(size_t, size_t), void (*_free)(void *)>
struct hashlistnode {
	_Key key;
	_Val val;
	struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *next;

	void * operator new(size_t size) {
		return _malloc(size);
	}

	void operator delete(void *p, size_t size) {
		_free(p);
	}

	void * operator new[](size_t size) {
		return _malloc(size);
	}

	void operator delete[](void *p, size_t size) {
		_free(p);
	}
};

/**
 * Hashtable class. By default it is snapshotting, but you can pass in your own
 * allocation functions.
 *
 * @tparam _Key    Type name for the key
 * @tparam _Val    Type name for the values to be stored
 * @tparam _KeyInt Integer type that is at least as large as _Key. Used for key
 *                 manipulation and storage.
 * @tparam _Shift  Logical shift to apply to all keys. Default 0.
 * @tparam _malloc Provide your own 'malloc' for the table, or default to
 *                 snapshotting.
 * @tparam _calloc Provide your own 'calloc' for the table, or default to
 *                 snapshotting.
 * @tparam _free   Provide your own 'free' for the table, or default to
 *                 snapshotting.
 */
template<typename _Key, typename _Val, typename _KeyInt, int _Shift=0, void * (* _malloc)(size_t)=malloc, void * (* _calloc)(size_t, size_t)=calloc, void (*_free)(void *)=free>
	class HashTable {
 public:
	/**
	 * Constructor
	 * @param initialcapacity Sets the initial capacity of the hash table.
	 * Default size 1024.
	 * @param factor Sets the percentage full before the hashtable is
	 * resized. Default ratio 0.5.
	 */
	HashTable(unsigned int initialcapacity=1024, double factor=0.5) {
		// Allocate space for the hash table
		table = (struct hashlistnode<_Key,_Val, _malloc, _calloc,_free> **) _calloc(initialcapacity, sizeof(struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *));
		loadfactor = factor;
		capacity = initialcapacity;
		threshold = (unsigned int) (initialcapacity*loadfactor);
		mask = (capacity << _Shift)-1;
		size = 0; // Initial number of elements in the hash
	}

	/** Destructor */
	~HashTable() {
		for(unsigned int i=0;i<capacity;i++) {
			struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> * bin = table[i];
			while(bin!=NULL) {
				struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> * next=bin->next;
				delete bin;
				bin=next;
			}
		}
		_free(table);
	}

	/** Override: new operator */
	void * operator new(size_t size) {
		return _malloc(size);
	}

	/** Override: delete operator */
	void operator delete(void *p, size_t size) {
		_free(p);
	}

	/** Override: new[] operator */
	void * operator new[](size_t size) {
		return _malloc(size);
	}

	/** Override: delete[] operator */
	void operator delete[](void *p, size_t size) {
		_free(p);
	}

	/** Reset the table to its initial state. */
	void reset() {
		for(unsigned int i=0;i<capacity;i++) {
			struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> * bin = table[i];
			while(bin!=NULL) {
				struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> * next=bin->next;
				delete bin;
				bin=next;
			}
		}
		memset(table, 0, capacity*sizeof(struct hashlistnode<_Key, _Val, _malloc, _calloc, _free> *));
		size=0;
	}

	/** Put a key value pair into the table. */
	void put(_Key key, _Val val) {
		if (size > threshold)
			resize(capacity << 1);

		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *ptr = table[(((_KeyInt)key) & mask)>>_Shift];
		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *search = ptr;

		while(search!=NULL) {
			if (search->key==key) {
				search->val=val;
				return;
			}
			search=search->next;
		}

		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *newptr=(struct hashlistnode<_Key,_Val,_malloc,_calloc,_free> *)new struct hashlistnode<_Key,_Val, _malloc, _calloc, _free>;
		newptr->key=key;
		newptr->val=val;
		newptr->next=ptr;
		table[(((_KeyInt)key)&mask)>>_Shift]=newptr;
		size++;
	}

	/** Put a key entry into the table. */
	_Val * get_safe_ptr(_Key key) {
		if (size > threshold)
			resize(capacity << 1);

		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *ptr = table[(((_KeyInt)key) & mask)>>_Shift];
		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *search = ptr;

		while(search!=NULL) {
			if (search->key==key) {
				return &search->val;
			}
			search=search->next;
		}

		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *newptr=(struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *)new struct hashlistnode<_Key,_Val, _malloc, _calloc, _free>;
		newptr->key=key;
		newptr->next=ptr;
		table[(((_KeyInt)key)&mask)>>_Shift]=newptr;
		size++;
		return &newptr->val;
	}

	/** Lookup the corresponding value for the given key. */
	_Val get(_Key key) {
		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *search = table[(((_KeyInt)key) & mask)>>_Shift];

		while(search!=NULL) {
			if (search->key==key) {
				return search->val;
			}
			search=search->next;
		}
		return (_Val)0;
	}

	/** Lookup the corresponding value for the given key. */
	_Val * getptr(_Key key) {
		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *search = table[(((_KeyInt)key) & mask)>>_Shift];

		while(search!=NULL) {
			if (search->key==key) {
				return & search->val;
			}
			search=search->next;
		}
		return (_Val *) NULL;
	}

	/** Check whether the table contains a value for the given key. */
	bool contains(_Key key) {
		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *search = table[(((_KeyInt)key) & mask)>>_Shift];

		while(search!=NULL) {
			if (search->key==key) {
				return true;
			}
			search=search->next;
		}
		return false;
	}

	/** Resize the table. */
	void resize(unsigned int newsize) {
		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> ** oldtable = table;
		struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> ** newtable;
		unsigned int oldcapacity = capacity;

		if((newtable = (struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> **) _calloc(newsize, sizeof(struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> *))) == NULL) {
			printf("Calloc error %s %d\n", __FILE__, __LINE__);
			exit(-1);
		}

		table = newtable;          //Update the global hashtable upon resize()
		capacity = newsize;
		threshold = (unsigned int) (newsize * loadfactor);
		mask = (newsize << _Shift)-1;

		for(unsigned int i = 0; i < oldcapacity; i++) {
			struct hashlistnode<_Key, _Val, _malloc, _calloc, _free> * bin = oldtable[i];

			while(bin!=NULL) {
				_Key key=bin->key;
				struct hashlistnode<_Key, _Val, _malloc, _calloc, _free> * next=bin->next;

				unsigned int index = (((_KeyInt)key) & mask) >>_Shift;
				struct hashlistnode<_Key, _Val, _malloc, _calloc, _free> * tmp=newtable[index];
				bin->next=tmp;
				newtable[index]=bin;
				bin = next;
			}
		}

		_free(oldtable);            //Free the memory of the old hash table
	}

 private:
	struct hashlistnode<_Key,_Val, _malloc, _calloc, _free> **table;
	unsigned int capacity;
	_KeyInt mask;
	unsigned int size;
	unsigned int threshold;
	double loadfactor;
};
#endif
