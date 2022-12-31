
PROG=watchsi
SERVICE=$(PROG).service

all: $(PROG)

install: /etc/systemd/system/$(SERVICE)
	$(INSTALL) -m 755 $(INSTALL_USER) $(PROG) $(BINDIR)/$(PROG)

/etc/systemd/system/$(SERVICE): $(SERVICE)
ifeq ($(ID),0)
        $(INSTALL) -m 644 $(INSTALL_USER) $< $@
        systemctl daemon-reload
        systemctl enable $(SERVICE)
endif

include ../../Makefile.sd
