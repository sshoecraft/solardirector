
NAME=$(shell basename $(shell pwd))

#PROGNAME=$(NAME)
SRCS=main.c config.c driver.c info.c jsfuncs.c
CFLAGS+=-DAGENT_NAME=\"$(NAME)\"

SERVICE_NAME=$(NAME)
SERVICE_DESC="+DESC+"

JS=yes
MQTT=yes
INFLUX=yes

include ../../Makefile.sd
