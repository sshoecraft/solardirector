
LIBNAME=$(shell basename $(shell pwd))
SRCS=session.c frame.c packet.c command.c channel.c value.c jsfuncs.c
SMANET=yes

CLEANFILES+=try mktab

include ../Makefile.sd

try: try.c $(ALIB) $(MYTP)
	$(CC) $(CFLAGS) -o $@ try.c $(MYTP) $(LIBS)

debug: try
	gdb ./try
