
PROGNAME=$(shell basename $(shell pwd))
SRCS=main.c config.c server.c devserver.c

#Restrict can IDs to SI frames (0x300-0x30F)
CFLAGS+=-DRDEV_SI_FRAMES

include ../../Makefile.sd

test: test.c devserver.c config.c $(DEPS)
	$(FULLCC) -o $@ $< $(LIBS)
