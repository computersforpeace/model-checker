/** @file hashtable.h
 *  @brief Hashtable.  Standard chained bucket variety.
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mymemory.h"
#include "common.h"

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
template<typename _Key, typename _Val>

struct hashlistnode {
	_Key key;
	_Val val;
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
template<typename _Key, typename _Val, typename _KeyInt, int _Shift = 0, void * (* _malloc)(size_t) = snapshot_malloc, void * (* _calloc)(size_t, size_t) = snapshot_calloc, void (*_free)(void *) = snapshot_free>
	class HashTable {
 public:
	/**
	 * Constructor
	 * @param initialcapacity Sets the initial capacity of the hash table.
	 * Default size 1024.
	 * @param factor Sets the percentage full before the hashtable is
	 * resized. Default ratio 0.5.
	 */
	HashTable(unsigned int initialcapacity = 1024, double factor = 0.5) {
		// Allocate space for the hash table
		table = (struct hashlistnode<_Key, _Val> *)_calloc(initialcapacity, sizeof(struct hashlistnode<_Key, _Val>));
		loadfactor = factor;
		capacity = initialcapacity;
		capacitymask = initialcapacity - 1;

		threshold = (unsigned int)(initialcapacity * loadfactor);
		size = 0; // Initial number of elements in the hash
	}

	/** Destructor */
	~HashTable() {
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
		memset(table, 0, capacity * sizeof(struct hashlistnode<_Key, _Val>));
		size = 0;
	}

	/** Put a key value pair into the table. */
	void put(_Key key, _Val val) {
		if (size > threshold)
			resize(capacity << 1);

		struct hashlistnode<_Key, _Val> *search;

		unsigned int index = ((_KeyInt)key) >> _Shift;
		do {
			index &= capacitymask;
			search = &table[index];
			if (search->key == key) {
				search->val = val;
				return;
			}
			index++;
		} while (search->key);

		search->key = key;
		search->val = val;
		size++;
	}

	/** Lookup the corresponding value for the given key. */
	_Val get(_Key key) const {
		struct hashlistnode<_Key, _Val> *search;

		unsigned int index = ((_KeyInt)key) >> _Shift;
		do {
			index &= capacitymask;
			search = &table[index];
			if (search->key == key)
				return search->val;
			index++;
		} while (search->key);
		return (_Val) 0;
	}

	/** Check whether the table contains a value for the given key. */
	bool contains(_Key key) const {
		struct hashlistnode<_Key, _Val> *search;

		unsigned int index = ((_KeyInt)key) >> _Shift;
		do {
			index &= capacitymask;
			search = &table[index];
			if (search->key == key)
				return true;
			index++;
		} while (search->key);
		return false;
	}

	/** Resize the table. */
	void resize(unsigned int newsize) {
		struct hashlistnode<_Key, _Val> *oldtable = table;
		struct hashlistnode<_Key, _Val> *newtable;
		unsigned int oldcapacity = capacity;

		if ((newtable = (struct hashlistnode<_Key, _Val> *) _calloc(newsize, sizeof(struct hashlistnode<_Key, _Val>))) == NULL) {
			model_print("calloc error %s %d\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}

		table = newtable;          //Update the global hashtable upon resize()
		capacity = newsize;
		capacitymask = newsize - 1;

		threshold = (unsigned int) (newsize * loadfactor);

		struct hashlistnode<_Key, _Val> *bin = &oldtable[0];
		struct hashlistnode<_Key, _Val> *lastbin = &oldtable[oldcapacity];
		for (; bin < lastbin; bin++) {
			_Key key = bin->key;

			struct hashlistnode<_Key, _Val> *search;

			unsigned int index = ((_KeyInt)key) >> _Shift;
			do {
				index &= capacitymask;
				search = &table[index];
				index++;
			} while (search->key);

			search->key = key;
			search->val = bin->val;
		}

		_free(oldtable);            //Free the memory of the old hash table
	}

 private:
	struct hashlistnode<_Key, _Val> *table;
	unsigned int capacity;
	unsigned int size;
	unsigned int capacitymask;
	unsigned int threshold;
	double loadfactor;
};
#endif
