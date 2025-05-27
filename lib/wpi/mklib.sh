#!/bin/sh

JS=$1

do_make() {
	set -x
	make -f Makefile.libwpi -j ${MAKE_JOBS} JS=${js} $*
}

for js in yes no
do
	do_make
done
