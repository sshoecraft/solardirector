#!/bin/sh

jobs=$(cat /proc/cpuinfo 2>/dev/null| grep -c ^processor)
test -z "$jobs" && jobs=2

for js in yes no
do
	for mqtt in yes no
	do
		for influx in yes no
		do
			make -f Makefile.libsd -j ${jobs} JS=${js} MQTT=${mqtt} INFLUX=${influx} $*
		done
	done
done
