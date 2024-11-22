

NAME=$(shell basename $(shell pwd))

PROGNAME=$(NAME)
SRCS=main.c driver.c
CFLAGS+=-DAGENT_NAME=\"$(NAME)\"

LIBS+=-lwiringPi

SERVICE_NAME=$(NAME)
SERVICE_DESC=Air Handler Agent

JS=no
MQTT=yes
INFLUX=yes

include ../../Makefile.sd
