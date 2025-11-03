#!/bin/bash

for js in yes no
do
	make -f Makefile.libsma -j ${MAKE_JOBS} JS=${js} $*
done
