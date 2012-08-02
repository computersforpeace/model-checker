include common.mk

MODEL_CC=libthreads.cc schedule.cc model.cc threads.cc librace.cc action.cc nodestack.cc clockvector.cc main.cc snapshot-interface.cc cyclegraph.cc datarace.cc impatomic.cc cmodelint.cc promise.cc
MODEL_O=libthreads.o schedule.o model.o threads.o librace.o action.o nodestack.o clockvector.o main.o snapshot-interface.o cyclegraph.o datarace.o impatomic.o cmodelint.o promise.o
MODEL_H=libthreads.h schedule.h common.h model.h threads.h librace.h action.h nodestack.h clockvector.h snapshot-interface.h cyclegraph.h hashtable.h datarace.h config.h include/impatomic.h include/cstdatomic include/stdatomic.h cmodelint.h promise.h

SHMEM_CC=snapshot.cc malloc.c mymemory.cc
SHMEM_O=snapshot.o malloc.o mymemory.o
SHMEM_H=snapshot.h snapshotimp.h mymemory.h

CPPFLAGS += -Iinclude -I.
LDFLAGS=-ldl -lrt
SHARED=-shared

all: $(LIB_SO)

debug: CPPFLAGS += -DCONFIG_DEBUG
debug: all

mac: CPPFLAGS += -D_XOPEN_SOURCE -DMAC
mac: LDFLAGS=-ldl
mac: SHARED=-Wl,-undefined,dynamic_lookup -dynamiclib
mac: all

docs: *.c *.cc *.h
	doxygen

$(LIB_SO): $(MODEL_O) $(MODEL_H) $(SHMEM_O) $(SHMEM_H)
	$(CXX) $(SHARED) -o $(LIB_SO) $(MODEL_O) $(SHMEM_O) $(LDFLAGS)

malloc.o: malloc.c
	$(CC) -fPIC -c malloc.c -DMSPACES -DONLY_MSPACES $(CPPFLAGS)

mymemory.o: mymemory.h snapshotimp.h snapshot.h mymemory.cc
	$(CXX) -fPIC -c mymemory.cc $(CPPFLAGS)

snapshot.o: mymemory.h snapshot.h snapshotimp.h snapshot.cc
	$(CXX) -fPIC -c snapshot.cc $(CPPFLAGS)

%.o: %.cc $(MODEL_H)
	$(CXX) -fPIC -c $< $(CPPFLAGS)

clean:
	rm -f *.o *.so

mrclean: clean
	rm -rf docs

tags::
	ctags -R
