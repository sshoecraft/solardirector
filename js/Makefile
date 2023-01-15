
include ../make.opts

ifeq ($(JS),yes)
LIBNAME=$(shell basename $(shell pwd))
SRCS=jsapi.c jsarena.c jsarray.c jsatom.c jsbool.c jscntxt.c jsdate.c jsdbgapi.c jsdhash.c jsdtoa.c jsemit.c jsexn.c jsfile.c jsfun.c jsgc.c jshash.c jsinterp.c jsinvoke.c jsiter.c jslog2.c jslong.c jsmath.c jsnum.c jsobj.c jsopcode.c jsparse.c jsprf.c jsregexp.c jsscan.c jsscope.c jsscript.c jsstr.c jsutil.c jsxdrapi.c jsxml.c prmjtime.c jsdtracef.c jsglobal.c jsjson.c jsengine.c jsconv.c jsprintf.c console.c jsclass.c jsws.c jsarraybuf.c jslock.c jsnavigator.c jsdocument.c jslocation.c
CFLAGS+=-DJS_THREADSAFE -DNARCISSUS

CLEANFILES+=jskwgen jsautokw.h

include ../Makefile.sd

jsautokw.h: jskwgen 
	./$< $@

jskwgen: jskwgen.c
	gcc $(CFLAGS) -o $@ $<

jsscan.c: jsautokw.h

jsnavigator.c: jsnavigator_defs.h
jsnavigator_defs.h: jsnavigator.h
	$(SRC_ROOT)/tools/mkjs.sh jsnavigator.h jsnavigator "" 1 ro
CLEANFILES+=jsnavigator_defs.h

jsdocument.c: jsdocument_defs.h
jsdocument_defs.h: jsdocument.h
	$(SRC_ROOT)/tools/mkjs.sh jsdocument.h jsdocument "" 1 ro
CLEANFILES+=jsdocument_defs.h

jslocation.c: jslocation_defs.h
jslocation_defs.h: jslocation.h
	$(SRC_ROOT)/tools/mkjs.sh jslocation.h jslocation "" 1 ro
CLEANFILES+=jslocation_defs.h

jsglobal.c: jswindow_defs.h
jswindow_defs.h: jswindow.h
	$(SRC_ROOT)/tools/mkjs.sh jswindow.h jswindow "((jswindow_t *)s->private)->" 1024 ro
CLEANFILES+=jswindow_defs.h

else
all install clean:
endif
