
SCRIPTS=*.js

all:
	./mklib.sh $(JS) $(MQTT) $(INFLUX) $(RELEASE)

include ../../Makefile.sd

install:
	./mklib.sh $(JS) $(MQTT) $(INFLUX) $(RELEASE) install
ifeq ($(JS),yes)
ifneq ("$(wildcard $(SCRIPTS))","")
	mkdir -p $(LIBDIR)/sd
	$(INSTALL) -m 755 $(INSTALL_USER) $(SCRIPTS) $(LIBDIR)/sd
endif
endif

clean:
	rm -rf .deps *.o *.so *.a

