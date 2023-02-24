#!/bin/sh

jobs=$(cat /proc/cpuinfo 2>/dev/null| grep -c ^processor)
test -z "$jobs" && jobs=2

for js in yes no
do
	make -f Makefile.libsma -j ${jobs} JS=${js} $*
done
