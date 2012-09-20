include ../common.mk

CPPFLAGS += -I.. -I../include

SRCS = $(wildcard *.c)
CPSRCS = $(wildcard *.cc)
OBJS = $(patsubst %.c,%.o,$(SRCS)) $(patsubst %.cc,%.o,$(CPSRCS))

all: $(OBJS)

%.o: %.c
	$(CC) -o $@ $< $(CPPFLAGS) -L.. -l$(LIB_NAME)

%.o: %.cc
	$(CXX) -o $@ $< $(CPPFLAGS) -L.. -l$(LIB_NAME)

clean::
	rm -f *.o
