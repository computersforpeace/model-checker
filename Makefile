include common.mk

OBJECTS = libthreads.o schedule.o model.o threads.o librace.o action.o \
	  nodestack.o clockvector.o main.o snapshot-interface.o cyclegraph.o \
	  datarace.o impatomic.o cmodelint.o \
	  snapshot.o malloc.o mymemory.o

CPPFLAGS += -Iinclude -I.
LDFLAGS=-ldl -lrt
SHARED=-shared

TESTS=test

program_H_SRCS := $(wildcard *.h) $(wildcard include/*.h)
program_C_SRCS := $(wildcard *.c) $(wildcard *.cc)
DEPS = make.deps

all: $(LIB_SO) $(DEPS) tests

$(DEPS): $(program_C_SRCS) $(program_H_SRCS)
	$(CXX) $(CPPFLAGS) -MM $(program_C_SRCS) > $(DEPS)

include $(DEPS)

debug: CPPFLAGS += -DCONFIG_DEBUG
debug: all

mac: CPPFLAGS += -D_XOPEN_SOURCE -DMAC
mac: LDFLAGS=-ldl
mac: SHARED=-Wl,-undefined,dynamic_lookup -dynamiclib
mac: all

docs: *.c *.cc *.h
	doxygen

$(LIB_SO): $(OBJECTS)
	$(CXX) $(SHARED) -o $(LIB_SO) $(OBJECTS) $(LDFLAGS)

malloc.o: malloc.c
	$(CC) -fPIC -c malloc.c -DMSPACES -DONLY_MSPACES $(CPPFLAGS)

%.o: %.cc
	$(CXX) -fPIC -c $< $(CPPFLAGS)

clean:
	rm -f *.o *.so
	$(MAKE) -C $(TESTS) clean

mrclean: clean
	rm -rf docs

tags::
	ctags -R

tests:: $(LIB_SO)
	$(MAKE) -C $(TESTS)
