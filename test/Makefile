include ../common.mk

CPPFLAGS += -I.. -I../include

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

all: $(OBJS)

%.o: %.c
	$(CC) -o $@ $< $(CPPFLAGS) -L.. -l$(LIB_NAME)

clean::
	rm -f *.o
