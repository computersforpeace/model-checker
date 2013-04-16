/** @file hashtable.h
 *  @brief Hashtable.  Standard chained bucket variety.
 */

#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mymemory.h"
#include "common.h"

/**
 * @brief HashTable node
 *
 * @tparam _Key    Type name for the key
 * @tparam _Val    Type name for the values to be stored
 */
template<typename _Key, typename _Val>
struct hashlistnode {
	_Key key;
	_Val val;
};

/**
 * @brief A simple, custom hash table
 *
 * By default it is snapshotting, but you can pass in your own allocation
 * functions. Note that this table does not support the value 0 (NULL) used as
 * a key and is designed primarily with pointer-based keys in mind. Other
 * primitive key types are supported only for non-zero values.
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
	 * @brief Hash table constructor
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

	/** @brief Hash table destructor */
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

	/** @brief Reset the table to its initial state. */
	void reset() {
		memset(table, 0, capacity * sizeof(struct hashlistnode<_Key, _Val>));
		size = 0;
	}

	/**
	 * @brief Put a key/value pair into the table
	 * @param key The key for the new value; must not be 0 or NULL
	 * @param val The value to store in the table
	 */
	void put(_Key key, _Val val) {
		/* HashTable cannot handle 0 as a key */
		ASSERT(key);

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

	/**
	 * @brief Lookup the corresponding value for the given key
	 * @param key The key for finding the value; must not be 0 or NULL
	 * @return The value in the table, if the key is found; otherwise 0
	 */
	_Val get(_Key key) const {
		struct hashlistnode<_Key, _Val> *search;

		/* HashTable cannot handle 0 as a key */
		ASSERT(key);

		unsigned int index = ((_KeyInt)key) >> _Shift;
		do {
			index &= capacitymask;
			search = &table[index];
			if (search->key == key)
				return search->val;
			index++;
		} while (search->key);
		return (_Val)0;
	}

	/**
	 * @brief Check whether the table contains a value for the given key
	 * @param key The key for finding the value; must not be 0 or NULL
	 * @return True, if the key is found; false otherwise
	 */
	bool contains(_Key key) const {
		struct hashlistnode<_Key, _Val> *search;

		/* HashTable cannot handle 0 as a key */
		ASSERT(key);

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

	/**
	 * @brief Resize the table
	 * @param newsize The new size of the table
	 */
	void resize(unsigned int newsize) {
		struct hashlistnode<_Key, _Val> *oldtable = table;
		struct hashlistnode<_Key, _Val> *newtable;
		unsigned int oldcapacity = capacity;

		if ((newtable = (struct hashlistnode<_Key, _Val> *)_calloc(newsize, sizeof(struct hashlistnode<_Key, _Val>))) == NULL) {
			model_print("calloc error %s %d\n", __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}

		table = newtable;          // Update the global hashtable upon resize()
		capacity = newsize;
		capacitymask = newsize - 1;

		threshold = (unsigned int)(newsize * loadfactor);

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

		_free(oldtable);            // Free the memory of the old hash table
	}

 private:
	struct hashlistnode<_Key, _Val> *table;
	unsigned int capacity;
	unsigned int size;
	unsigned int capacitymask;
	unsigned int threshold;
	double loadfactor;
};

#endif /* __HASHTABLE_H__ */
