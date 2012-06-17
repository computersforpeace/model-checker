#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdlib.h>
#include <stdio.h>

template<typename _Key, typename _Val>
	struct hashlistnode {
		_Key key;
		_Val val;
		struct hashlistnode<_Key,_Val> *next;
	};

template<typename _Key, typename _Val, int _Shift>
	class HashTable {
 public:
	HashTable(unsigned int initialcapacity, double factor) {
		// Allocate space for the hash table
		table = calloc(initialcapacity, sizeof(struct hashlistnode<_Key,_Val> *));
		loadfactor = factor;
		capacity = initialcapacity;
		threshold = initialcapacity*loadfactor;
		mask = (capacity << _Shift)-1;
		size = 0; // Initial number of elements in the hash
	}

	~HashTable() {
		free(table);
	}
	
	void insert(_Key key, _Val val) {
		if(size > threshold) {
			//Resize
			unsigned int newsize = capacity << 1;
			resize(newsize);
		}
		
		struct hashlistnode<_Key,_Val> *ptr = table[(key & mask)>>_Shift];
		size++;
		struct hashlistnode<_Key,_Val> *search = ptr;

		while(search!=NULL) {
			if (search->key==key) {
				search->val=val;
				return;
			}
			search=search->next;
		}

		struct hashlistnode<_Key,_Val> *newptr=malloc(sizeof(struct hashlistnode<_Key,_Val>));
		newptr->key=key;
		newptr->val=val;
		newptr->next=ptr;
		table[(key&mask)>>_Shift]=newptr;
	}

	_Val get(_Key key) {
		struct hashlistnode<_Key,_Val> *search = table[(key & mask)>>_Shift];
		
		while(search!=NULL) {
			if (search->key==key) {
				return search->val;
			}
			search=search->next;
		}
		return (_Val)0;
	}
	
	void resize(unsigned int newsize) {
		struct hashlistnode<_Key,_Val> ** oldtable = table;
		struct hashlistnode<_Key,_Val> ** newtable;
		unsigned int oldcapacity = capacity;
		
		if((newtable = calloc(newsize, sizeof(struct hashlistnode<_Key,_Val> *))) == NULL) {
			printf("Calloc error %s %d\n", __FILE__, __LINE__);
			exit(-1);
		}
		
		table = newtable;          //Update the global hashtable upon resize()
		capacity = newsize;
		threshold = newsize * loadfactor;
		mask = (newsize << _Shift)-1;
		
		for(unsigned int i = 0; i < oldcapacity; i++) {
			struct hashlistnode<_Key, _Val> * bin = oldtable[i];
			
			while(bin!=NULL) {
				_Key key=bin->key;
				struct hashlistnode<_Key, _Val> * next=bin->next;
				
				unsigned int index = (key & mask) >>_Shift;
				struct hashlistnode<_Key, _Val> tmp=newtable[index];
				bin->next=tmp;
				newtable[index]=bin;
				bin = next;
			}
		}
		
		free(oldtable);            //Free the memory of the old hash table
	}
	
 private:
	struct hashlistnode<_Key,_Val> **table;
	unsigned int capacity;
	_Key mask;
	unsigned int size;
	unsigned int threshold;
	double loadfactor;
};
#endif
