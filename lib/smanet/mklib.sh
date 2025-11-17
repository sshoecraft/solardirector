#!/bin/bash

# Set MAKE_JOBS if not already set
if [ -z "$MAKE_JOBS" ]; then
	if command -v nproc >/dev/null 2>&1; then
		MAKE_JOBS=$(nproc)
	elif [ -f /proc/cpuinfo ]; then
		MAKE_JOBS=$(grep -c ^processor /proc/cpuinfo)
	else
		MAKE_JOBS=4
	fi
fi

for js in yes no
do
	make -f Makefile.libsma -j ${MAKE_JOBS} JS=${js} $*
done
