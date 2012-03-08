BIN=libthreads
SOURCE=libthreads.c
HEADERS=libthreads.h
FLAGS=

all: ${BIN}

${BIN}: ${SOURCE} ${HEADERS}
	gcc -o ${BIN} ${SOURCE} ${FLAGS}

clean:
	rm -f ${BIN} *.o
