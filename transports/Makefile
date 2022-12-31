
include ../make.opts

LIBNAME=$(shell basename $(shell pwd))
SRCS+=null.c can.c rdev.c ip.c serial.c http.c

ifeq ($(BLUETOOTH),yes)
	SRCS+=bt.c
	SHAREDLIBS+=-lgattlib
endif

include ../Makefile.sd
