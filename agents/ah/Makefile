

NAME=$(shell basename $(shell pwd))

PROGNAME=$(NAME)
SRCS=main.c driver.c
CFLAGS+=-DAGENT_NAME=\"$(NAME)\"
CFLAGS+=-DSIM

#SERVICE_NAME=$(NAME)
#SERVICE_DESC=Air Handler Agent

JS=yes
MQTT=yes
INFLUX=yes
WPI=yes

include ../../Makefile.sd
