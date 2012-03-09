BIN=libthreads
SOURCE=libthreads.c schedule.c
HEADERS=libthreads.h schedule.h
FLAGS=

all: ${BIN}

${BIN}: ${SOURCE} ${HEADERS}
	gcc -o ${BIN} ${SOURCE} ${FLAGS}

clean:
	rm -f ${BIN} *.o

tags::
	ctags -R
