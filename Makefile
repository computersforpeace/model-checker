include common.mk

OBJECTS = libthreads.o schedule.o model.o threads.o librace.o action.o \
	  nodestack.o clockvector.o main.o snapshot-interface.o cyclegraph.o \
	  datarace.o impatomic.o cmodelint.o \
	  snapshot.o malloc.o mymemory.o common.o mutex.o promise.o conditionvariable.o \
	  context.o scanalysis.o execution.o

CPPFLAGS += -Iinclude -I.
LDFLAGS = -ldl -lrt -rdynamic
SHARED = -shared

# Mac OSX options
ifeq ($(UNAME), Darwin)
LDFLAGS = -ldl
SHARED = -Wl,-undefined,dynamic_lookup -dynamiclib
endif

TESTS_DIR = test

all: $(LIB_SO) tests

debug: CPPFLAGS += -DCONFIG_DEBUG
debug: all

PHONY += docs
docs: *.c *.cc *.h
	doxygen

$(LIB_SO): $(OBJECTS)
	$(CXX) $(SHARED) -o $(LIB_SO) $+ $(LDFLAGS)

malloc.o: malloc.c
	$(CC) -fPIC -c malloc.c -DMSPACES -DONLY_MSPACES -DHAVE_MMAP=0 $(CPPFLAGS) -Wno-unused-variable

%.o: %.cc
	$(CXX) -MMD -MF .$@.d -fPIC -c $< $(CPPFLAGS)

%.pdf: %.dot
	dot -Tpdf $< -o $@

-include $(OBJECTS:%=.%.d)

PHONY += clean
clean:
	rm -f *.o *.so .*.d *.pdf *.dot
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

PHONY += pdfs
pdfs: $(patsubst %.dot,%.pdf,$(wildcard *.dot))

.PHONY: $(PHONY)
