#!/bin/sh
#
# Runs a simple test (default: ./test/userprog.o)
# Syntax:
#  ./run.sh [gdb]
#  ./run.sh [test program] [gdb]
#
# If you include a 'gdb' argument, the your program will be launched with gdb.
# You can also supply a test program argument to run something besides the
# default program.
#

BIN=./test/userprog.o

export LD_LIBRARY_PATH=.

[ $# -gt 0 ] && [ "$1" != "gdb" ] && BIN=$1 && shift

if [ $# -gt 0 ] && [ "$1" = "gdb" ]; then
	shift
	gdb $BIN $@
fi

$BIN $@
