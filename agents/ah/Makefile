

NAME=$(shell basename $(shell pwd))

PROGNAME=$(NAME)
SRCS=main.c config.c driver.c info.c
CFLAGS+=-DAGENT_NAME=\"$(NAME)\"

LIBS+=-lwiringPi

SERVICE_NAME=$(NAME)
SERVICE_DESC=Air Handler Agent

JS=yes
MQTT=yes
INFLUX=yes

include ../../Makefile.sd
