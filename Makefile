CC=gcc
BIN=libthreads
SOURCE=libthreads.c schedule.c libatomic.c userprog.c model.c
HEADERS=libthreads.h schedule.h common.h libatomic.h model.h
FLAGS=-Wall

all: ${BIN}

${BIN}: ${SOURCE} ${HEADERS}
	${CC} -o ${BIN} ${SOURCE} ${FLAGS}

clean:
	rm -f ${BIN} *.o

tags::
	ctags -R
