
SMANET=yes

PROGNAME=$(shell basename $(shell pwd))
SRCS=main.c

WPI=yes

include ../../Makefile.sd

ifneq ($(OS),Windows)
LIBS+=-lreadline
endif

