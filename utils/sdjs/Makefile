
SMANET=yes

PROGNAME=$(shell basename $(shell pwd))
SRCS=main.c

include ../../Makefile.sd

ifneq ($(OS),Windows)
LIBS+=-lreadline
endif

