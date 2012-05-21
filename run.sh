#!/bin/sh
export LD_LIBRARY_PATH=.

if [ $# -gt 0 ]; then
	if [ "$1" = "gdb" ]; then
		gdb ./model
	else
		echo "Invalid argument(s)"
		exit 1
	fi
else
	./model
fi
