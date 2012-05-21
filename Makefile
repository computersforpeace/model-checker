CC=gcc
CXX=g++

BIN=model
LIB_NAME=model
LIB_SO=lib$(LIB_NAME).so

USER_O=userprog.o
USER_H=libthreads.h libatomic.h

MODEL_CC=libthreads.cc schedule.cc libatomic.cc model.cc threads.cc librace.cc action.cc nodestack.cc clockvector.cc main.cc snapshot-interface.cc
MODEL_O=libthreads.o schedule.o libatomic.o model.o threads.o librace.o action.o nodestack.o clockvector.o main.o snapshot-interface.o
MODEL_H=libthreads.h schedule.h common.h libatomic.h model.h threads.h librace.h action.h nodestack.h clockvector.h snapshot-interface.h

SHMEM_CC=snapshot.cc malloc.c mymemory.cc
SHMEM_O=snapshot.o malloc.o mymemory.o
SHMEM_H=snapshot.h snapshotimp.h mymemory.h

CPPFLAGS=-Wall -g
LDFLAGS=-ldl -lrt

all: $(BIN)

$(BIN): $(USER_O) $(LIB_SO)
	$(CXX) -o $(BIN) $(USER_O) -L. -l$(LIB_NAME)

# note: implicit rule for generating $(USER_O) (i.e., userprog.c -> userprog.o)

$(LIB_SO): $(MODEL_O) $(MODEL_H) $(SHMEM_O) $(SHMEM_H)
	$(CXX) -shared -o $(LIB_SO) $(MODEL_O) $(SHMEM_O) $(LDFLAGS)

malloc.o: malloc.c
	$(CC) -fPIC -c malloc.c -DMSPACES -DONLY_MSPACES $(CPPFLAGS)

mymemory.o: mymemory.h snapshotimp.h mymemory.cc
	$(CXX) -fPIC -c mymemory.cc $(CPPFLAGS)

snapshot.o: mymemory.h snapshot.h snapshotimp.h snapshot.cc
	$(CXX) -fPIC -c snapshot.cc $(CPPFLAGS)

$(MODEL_O): $(MODEL_CC) $(MODEL_H)
	$(CXX) -fPIC -c $(MODEL_CC) $(CPPFLAGS)

clean:
	rm -f $(BIN) *.o *.so

tags::
	ctags -R
