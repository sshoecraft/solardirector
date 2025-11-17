#!/bin/sh

JS=$1

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

do_make() {
	set -x
	make -f Makefile.libwpi -j ${MAKE_JOBS} JS=${js} $*
}

for js in yes no
do
	do_make
done
