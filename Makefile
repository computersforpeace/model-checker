CC=gcc
CXX=g++

BIN=model
LIB_NAME=model
LIB_SO=lib$(LIB_NAME).so

USER_O=userprog.o
USER_H=libthreads.h libatomic.h

MODEL_CC=libthreads.cc schedule.cc libatomic.cc model.cc malloc.c threads.cc tree.cc librace.cc action.cc clockvector.cc main.cc
MODEL_O=libthreads.o schedule.o libatomic.o model.o malloc.o threads.o tree.o librace.o action.o clockvector.o main.o
MODEL_H=libthreads.h schedule.h common.h libatomic.h model.h threads.h tree.h librace.h action.h clockvector.h

CPPFLAGS=-Wall -g
LDFLAGS=-ldl

all: $(BIN)

$(BIN): $(USER_O) $(LIB_SO)
	$(CXX) -o $(BIN) $(USER_O) -L. -l$(LIB_NAME) $(CPPFLAGS)

# note: implicit rule for generating $(USER_O) (i.e., userprog.c -> userprog.o)

$(LIB_SO): $(MODEL_O) $(MODEL_H)
	$(CXX) -shared -Wl,-soname,$(LIB_SO) -o $(LIB_SO) $(MODEL_O) $(LDFLAGS) $(CPPFLAGS)

$(MODEL_O): $(MODEL_CC) $(MODEL_H)
	$(CXX) -fPIC -c $(MODEL_CC) $(CPPFLAGS)

clean:
	rm -f $(BIN) *.o *.so

tags::
	ctags -R
