CC=g++
BIN=libthreads
SOURCE=libthreads.cc schedule.cc libatomic.cc userprog.c model.cc malloc.c
HEADERS=libthreads.h schedule.h common.h libatomic.h model.h
FLAGS=-Wall -ldl

all: ${BIN}

${BIN}: ${SOURCE} ${HEADERS}
	${CC} -o ${BIN} ${SOURCE} ${FLAGS}

clean:
	rm -f ${BIN} *.o

tags::
	ctags -R
