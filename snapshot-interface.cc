#include "snapshot-interface.h"
#include "snapshot.h"
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sstream>
#include <cstring>
#include <string>
#include <cassert>
#include <vector>
#include <utility>
#include <inttypes.h>
#include "common.h"


#define MYBINARYNAME "model"
#define MYLIBRARYNAME "libmodel.so"
#define MAPFILE_FORMAT "/proc/%d/maps"

SnapshotStack * snapshotObject;

#ifdef MAC
void SnapshotGlobalSegments(){
	int pid = getpid();
	char buf[9000], execname[100];
	FILE *map;

	sprintf(execname, "vmmap -interleaved %d", pid);
	map=popen(execname, "r");

	if (!map) {
		perror("popen");
		exit(EXIT_FAILURE);
	}

	/* Wait for correct part */
	while (fgets(buf, sizeof(buf), map)) {
		if (strstr(buf, "==== regions for process"))
			break;
	}

	while (fgets(buf, sizeof(buf), map)) {
		char regionname[200] = "";
		char type[23];
		char smstr[23];
		char r, w, x;
		char mr, mw, mx;
		int size;
		void *begin, *end;

		//Skip out at the end of the section
		if (buf[0]=='\n')
			break;
		
		sscanf(buf, "%22s %p-%p [%5dK] %c%c%c/%c%c%c SM=%3s %200s\n", &type, &begin, &end, &size, &r, &w, &x, &mr, &mw, &mx, smstr, regionname);

		if (w == 'w' && (strstr(regionname, MYBINARYNAME) || strstr(regionname, MYLIBRARYNAME))) {
			size_t len = ((uintptr_t)end - (uintptr_t)begin) / PAGESIZE;
			if (len != 0)
				addMemoryRegionToSnapShot(begin, len);
			DEBUG("%s\n", buf);
			DEBUG("%45s: %18p - %18p\t%c%c%c%c\n", regionname, begin, end, r, w, x, p);
		}
	}
	pclose(map);
}
#else
void SnapshotGlobalSegments(){
	int pid = getpid();
	char buf[9000], filename[100];
	FILE *map;

	sprintf(filename, MAPFILE_FORMAT, pid);
	map = fopen(filename, "r");
	if (!map) {
		perror("fopen");
		exit(EXIT_FAILURE);
	}
	while (fgets(buf, sizeof(buf), map)) {
		char regionname[200] = "";
		char r, w, x, p;
		void *begin, *end;

		sscanf(buf, "%p-%p %c%c%c%c %*x %*x:%*x %*u %200s\n", &begin, &end, &r, &w, &x, &p, regionname);
		if (w == 'w' && (strstr(regionname, MYBINARYNAME) || strstr(regionname, MYLIBRARYNAME))) {
			size_t len = ((uintptr_t)end - (uintptr_t)begin) / PAGESIZE;
			if (len != 0)
				addMemoryRegionToSnapShot(begin, len);
			DEBUG("%45s: %18p - %18p\t%c%c%c%c\n", regionname, begin, end, r, w, x, p);
		}
	}
	fclose(map);
}
#endif

//class definition of SnapshotStack.....
//declaration of constructor....
SnapshotStack::SnapshotStack(){
	SnapshotGlobalSegments();
	stack=NULL;
}

SnapshotStack::~SnapshotStack(){
}

int SnapshotStack::backTrackBeforeStep(int seqindex) {
	while(true) {
		if (stack->index<=seqindex) {
			//have right entry
			rollBack(stack->snapshotid);
			return stack->index;
		}
		struct stackEntry *tmp=stack;
		MYFREE(tmp);
		stack=stack->next;
	}
}

void SnapshotStack::snapshotStep(int seqindex) {
	struct stackEntry *tmp=(struct stackEntry *)MYMALLOC(sizeof(struct stackEntry));
	tmp->next=stack;
	tmp->index=seqindex;
	tmp->snapshotid=takeSnapshot();
	stack=tmp;
}
