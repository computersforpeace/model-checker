CC=gcc
CXX=g++

BIN=model
LIB_NAME=model
LIB_MEM=mymemory
LIB_SO=lib$(LIB_NAME).so
LIB_MEM_SO=lib$(LIB_MEM).so

USER_O=userprog.o
USER_H=libthreads.h libatomic.h

MODEL_CC=libthreads.cc schedule.cc libatomic.cc model.cc threads.cc tree.cc librace.cc action.cc nodestack.cc clockvector.cc main.cc snapshot-interface.cc mallocwrap.cc
MODEL_O=libthreads.o schedule.o libatomic.o model.o threads.o tree.o librace.o action.o nodestack.o clockvector.o main.o snapshot-interface.o mallocwrap.o
MODEL_H=libthreads.h schedule.h common.h libatomic.h model.h threads.h tree.h librace.h action.h nodestack.h clockvector.h snapshot-interface.h

SHMEM_CC=snapshot.cc malloc.c mymemory.cc
SHMEM_O=snapshot.o malloc.o mymemory.o
SHMEM_H=snapshot.h snapshotimp.h mymemory.h

CPPFLAGS=-Wall -g
LDFLAGS=-ldl -lrt

MEMCPPFLAGS=-fPIC -g -c -Wall
all: $(BIN)

$(BIN): $(USER_O) $(LIB_SO) $(LIB_MEM_SO)
	$(CXX) -o $(BIN) $(USER_O) -L. -l$(LIB_NAME) -l$(LIB_MEM) $(CPPFLAGS)

# note: implicit rule for generating $(USER_O) (i.e., userprog.c -> userprog.o)

$(LIB_SO): $(MODEL_O) $(MODEL_H)
	$(CXX) -shared -Wl,-soname,$(LIB_SO) -o $(LIB_SO) $(MODEL_O) $(LDFLAGS) $(CPPFLAGS)

$(LIB_MEM_SO): $(SHMEM_O) $(SHMEM_H)
	$(CC) -shared -W1,rpath,"." -o $(LIB_MEM_SO) $(SHMEM_O)

malloc.o: malloc.c
	$(CC) $(MEMCPPFLAGS) -DMSPACES -DONLY_MSPACES malloc.c

mymemory.o: mymemory.h snapshotimp.h mymemory.cc
	$(CXX) $(MEMCPPFLAGS) mymemory.cc 

snapshot.o: mymemory.h snapshot.h snapshotimp.h snapshot.cc
	$(CXX) $(MEMCPPFLAGS) snapshot.cc

$(MODEL_O): $(MODEL_CC) $(MODEL_H)
	$(CXX) -fPIC -c $(MODEL_CC) $(CPPFLAGS)

clean:
	rm -f $(BIN) *.o *.so

tags::
	ctags -R
