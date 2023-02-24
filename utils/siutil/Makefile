
PROGNAME=$(shell basename $(shell pwd))
SRCS=main.c driver.c can.c smanet.c info.c display.c

JS=no
MQTT=no
INFLUX=no
SMANET=yes

include ../../Makefile.sd

vpath %.c $(SRC_ROOT)/agents/si
CFLAGS+=-I $(SRC_ROOT)/agents/si
