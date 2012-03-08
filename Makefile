BIN=libthreads
SOURCE=libthreads.c
FLAGS=

all: ${BIN}

${BIN}: ${SOURCE}
	gcc -o ${BIN} ${SOURCE} ${FLAGS}

clean:
	rm -f ${BIN} *.o
