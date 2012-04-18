CC=g++
BIN=model
SOURCE=libthreads.cc schedule.cc libatomic.cc userprog.c model.cc malloc.c threads.cc tree.cc
HEADERS=libthreads.h schedule.h common.h libatomic.h model.h threads.h tree.h
FLAGS=-Wall -ldl -g

all: ${BIN}

${BIN}: ${SOURCE} ${HEADERS}
	${CC} -o ${BIN} ${SOURCE} ${FLAGS}

clean:
	rm -f ${BIN} *.o

tags::
	ctags -R
