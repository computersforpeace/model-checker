CC=g++
BIN=libthreads
SOURCE=libthreads.c schedule.cc libatomic.c userprog.c model.cc
HEADERS=libthreads.h schedule.h common.h libatomic.h model.h
FLAGS=-Wall

all: ${BIN}

${BIN}: ${SOURCE} ${HEADERS}
	${CC} -o ${BIN} ${SOURCE} ${FLAGS}

clean:
	rm -f ${BIN} *.o

tags::
	ctags -R
