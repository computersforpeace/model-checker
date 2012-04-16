CC=g++
BIN=model
SOURCE=libthreads.cc schedule.cc libatomic.cc userprog.c model.cc malloc.c threads.cc
HEADERS=libthreads.h schedule.h common.h libatomic.h model.h threads_internal.h
FLAGS=-Wall -ldl -g

all: ${BIN}

${BIN}: ${SOURCE} ${HEADERS}
	${CC} -o ${BIN} ${SOURCE} ${FLAGS}

clean:
	rm -f ${BIN} *.o

tags::
	ctags -R
