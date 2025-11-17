#!/bin/bash

JS=$1
MQTT=$2
INFLUX=$3
RELEASE=$4
test -z "$RELEASE" && RELEASE=no
INSTALL=$5

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
	make -f Makefile.libsd -j ${MAKE_JOBS} RELEASE=${RELEASE} JS=${js} MQTT=${mqtt} INFLUX=${influx} $INSTALL $*
}

do_influx() {
	if test "$INFLUX" = "yes"; then
		for influx in yes no
		do
			do_make
		done
	else
		influx=no
		do_make
	fi
}

do_mqtt() {
	if test "$MQTT" = "yes"; then
		for mqtt in yes no
		do
			do_influx
		done
	else
		mqtt=no
		do_influx
	fi
}

if test "$JS" = "yes"; then
	for js in yes no
	do
		do_mqtt
	done
else
	js=no
	do_mqtt
fi
