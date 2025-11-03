
NAME=$(shell basename $(shell pwd))

PROGNAME=$(NAME)
SRCS=main.c driver.c jsfuncs.c
SCRIPTS=*.js
CFLAGS+=-DAGENT_NAME=\"$(NAME)\"

SERVICE_NAME=$(NAME)

JS=yes
MQTT=yes
INFLUX=yes

include ../../Makefile.sd
