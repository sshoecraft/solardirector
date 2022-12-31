
include ../Makefile.sd

ifeq ($(ID),0)
ifeq ("$(wildcard /etc/profile.d/sd.sh)","")
SH=/etc/profile.d/sd.sh
endif

ifeq ("$(wildcard /etc/sd.env)","")
ENV=/etc/sd.env
endif

ifeq ("$(wildcard /etc/sd.conf)","")
CONF=/etc/sd.conf
endif
else
CONF=$(HOME)/.sd.conf
endif

sd.sh: sd.sh.template
	cat $< | sed -e "s:+PREFIX+:$(PREFIX):" \
		-e "s:+BINDIR+:$(BINDIR):" \
		-e "s:+ETCDIR+:$(ETCDIR):" \
		-e "s:+LIBDIR+:$(LIBDIR):" \
		-e "s:+LOGDIR+:$(LOGDIR):" \
		> $@

sd.env: sd.env.template
	cat $< | sed -e "s:+PREFIX+:$(PREFIX):" \
		-e "s:+BINDIR+:$(BINDIR):" \
		-e "s:+ETCDIR+:$(ETCDIR):" \
		-e "s:+LIBDIR+:$(LIBDIR):" \
		-e "s:+LOGDIR+:$(LOGDIR):" \
		> $@

sd.conf: sd.conf.template
	cat $< | sed -e "s:+PREFIX+:$(PREFIX):" \
		-e "s:+BINDIR+:$(BINDIR):" \
		-e "s:+ETCDIR+:$(ETCDIR):" \
		-e "s:+LIBDIR+:$(LIBDIR):" \
		-e "s:+LOGDIR+:$(LOGDIR):" \
		> $@

all: sd.conf sd.env sd.sh

install: $(CONF) $(ENV) $(SH)

CLEANFILES+=sd.conf sd.sh

$(SH): sd.sh
	$(INSTALL) -m 644 $(INSTALL_USER) $< $@

$(ENV): sd.env
	$(INSTALL) -m 644 $(INSTALL_USER) $< $@

$(CONF): sd.conf
	$(INSTALL) -m 644 $(INSTALL_USER) $< $@

