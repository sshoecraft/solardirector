
include ../make.opts

LIBNAME=core
SRCS=common.c debug.c opts.c log.c utils.c battery.c pvinverter.c getpath.c daemon.c driver.c homedir.c list.c types.c conv.c json.c cfg.c mqtt.c uuid.c message.c findconf.c buffer.c tmpdir.c exec.c agent.c client.c fork.c config.c notify.c dns.c influx.c stredit.c websocket.c event.c
SCRIPTS=*.js

ifeq ($(DEBUG_MEM),yes)
	ifeq ($(MEMWATCH),yes)
		SRCS+=memwatch.c
	else
		SRCS+=debugmem.c
	endif
endif

include ../Makefile.sd
