#!/bin/sh

JS=$1
MQTT=$2
INFLUX=$3
RELEASE=$4
test -z "$RELEASE" && RELEASE=no
INSTALL=$5

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
