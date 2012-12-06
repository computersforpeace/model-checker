include common.mk

OBJECTS = libthreads.o schedule.o model.o threads.o librace.o action.o \
	  nodestack.o clockvector.o main.o snapshot-interface.o cyclegraph.o \
	  datarace.o impatomic.o cmodelint.o \
	  snapshot.o malloc.o mymemory.o common.o mutex.o promise.o conditionvariable.o

CPPFLAGS += -Iinclude -I.
LDFLAGS = -ldl -lrt -rdynamic
SHARED = -shared

# Mac OSX options
ifeq ($(UNAME), Darwin)
LDFLAGS = -ldl
SHARED = -Wl,-undefined,dynamic_lookup -dynamiclib
endif

TESTS_DIR = test

program_H_SRCS := $(wildcard *.h) $(wildcard include/*.h)
program_C_SRCS := $(wildcard *.c) $(wildcard *.cc)
DEPS = make.deps

all: $(LIB_SO) tests

$(DEPS): build_deps := 1
$(DEPS): $(program_C_SRCS) $(program_H_SRCS)
	$(CXX) -MM $(program_C_SRCS) $(CPPFLAGS) > $(DEPS)

ifeq ($(build_deps),1)
include $(DEPS)
endif

debug: CPPFLAGS += -DCONFIG_DEBUG
debug: all

PHONY += docs
docs: *.c *.cc *.h
	doxygen

$(LIB_SO): $(OBJECTS)
	$(CXX) $(SHARED) -o $(LIB_SO) $(OBJECTS) $(LDFLAGS)

malloc.o: malloc.c
	$(CC) -fPIC -c malloc.c -DMSPACES -DONLY_MSPACES -DHAVE_MMAP=0 $(CPPFLAGS) -Wno-unused-variable

%.o: %.cc $(DEPS)
	$(CXX) -fPIC -c $< $(CPPFLAGS)

PHONY += clean
clean:
	rm -f *.o *.so
	$(MAKE) -C $(TESTS_DIR) clean

PHONY += mrclean
mrclean: clean
	rm -rf docs

PHONY += tags
tags:
	ctags -R

PHONY += tests
tests: $(LIB_SO)
	$(MAKE) -C $(TESTS_DIR)

BENCH_DIR := benchmarks

PHONY += benchmarks
benchmarks: $(LIB_SO)
	@if ! test -d $(BENCH_DIR); then \
		echo "Directory $(BENCH_DIR) does not exist" && \
		echo "Please clone the benchmarks repository" && \
		echo && \
		exit 1; \
	fi
	$(MAKE) -C $(BENCH_DIR)

.PHONY: $(PHONY)
