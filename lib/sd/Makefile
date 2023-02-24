
SCRIPTS=*.js

all:
	./mklib.sh

include ../../Makefile.sd

install:
	./mklib.sh $@
ifeq ($(JS),yes)
ifneq ("$(wildcard $(SCRIPTS))","")
	mkdir -p $(LIBDIR)/sd
	$(INSTALL) -m 755 $(INSTALL_USER) $(SCRIPTS) $(LIBDIR)/sd
endif
endif

clean:
	rm -rf .deps *.o *.so *.a

