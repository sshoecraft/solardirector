
PROGNAME=$(shell basename $(shell pwd))
SRCS=main.c driver.c can.c smanet.c info.c display.c

SMANET=$(shell cat ../../agents/si/Makefile | grep ^SMANET= | awk -F= '{ print $2 }')
SMANET=yes

include ../../Makefile.sd

vpath %.c $(SRC_ROOT)/agents/si
CFLAGS+=-I $(SRC_ROOT)/agents/si
