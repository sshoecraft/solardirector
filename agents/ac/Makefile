
PROGNAME=ac
SRCS=main.c driver.c can.c jsfuncs.c
SCRIPTS=*.js

SERVICE_NAME=ac
SERVICE_DESC=AC Agent
SERVICE_ARGS=-i -a

JS=yes
MQTT=yes
INFLUX=yes
WPI=yes

include ../../Makefile.sd

# Run JS unit tests under sdjs. No build deps — tests load production .js
# files directly with a stubbed agent runtime (see test/harness.js).
SDJS ?= sdjs
test:
	@fail=0; for f in test/test_*.js; do \
		echo "==> $$f"; \
		$(SDJS) -U512K $$f || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "TESTS FAILED"; exit 1; fi; \
	echo "All tests passed"
.PHONY: test

full:
	systemctl stop ac; make install && systemctl start ac
