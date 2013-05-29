BASE := ..

OBJECTS := $(patsubst %.c, %.o, $(wildcard *.c))
OBJECTS += $(patsubst %.cc, %.o, $(wildcard *.cc))

include $(BASE)/common.mk

DIR := litmus
include $(DIR)/Makefile

CPPFLAGS += -I$(BASE) -I$(BASE)/include

all: $(OBJECTS)

%.o: %.c
	$(CC) -o $@ $< $(CPPFLAGS) -L$(BASE) -l$(LIB_NAME)

%.o: %.cc
	$(CXX) -o $@ $< $(CPPFLAGS) -L$(BASE) -l$(LIB_NAME)

clean::
	rm -f $(OBJECTS)
