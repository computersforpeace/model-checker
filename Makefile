CC=gcc
BIN=libthreads
SOURCE=libthreads.c schedule.c
HEADERS=libthreads.h schedule.h common.h
FLAGS=

all: ${BIN}

${BIN}: ${SOURCE} ${HEADERS}
	${CC} -o ${BIN} ${SOURCE} ${FLAGS}

clean:
	rm -f ${BIN} *.o

tags::
	ctags -R
