BASE = ..

include $(BASE)/common.mk

CPPFLAGS += -I$(BASE) -I$(BASE)/include

SRCS = $(wildcard *.c)
CPSRCS = $(wildcard *.cc)
OBJS = $(patsubst %.c,%.o,$(SRCS)) $(patsubst %.cc,%.o,$(CPSRCS))

all: $(OBJS) litmus-tests

litmus-tests::
	$(MAKE) -C litmus

%.o: %.c
	$(CC) -o $@ $< $(CPPFLAGS) -L$(BASE) -l$(LIB_NAME)

%.o: %.cc
	$(CXX) -o $@ $< $(CPPFLAGS) -L$(BASE) -l$(LIB_NAME)

clean::
	rm -f *.o
	$(MAKE) -C litmus clean
