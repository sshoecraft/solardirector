# Detect CPU count
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	MAKE_JOBS ?= $(shell sysctl -n hw.ncpu)
else
	MAKE_JOBS ?= $(shell nproc 2>/dev/null || grep -c ^processor /proc/cpuinfo)
endif
export MAKE_JOBS

all:
	@for d in *; do test "$$d" = "lib" && continue; echo $$d; if test -d $$d; then make -j $(MAKE_JOBS) -C $$d || exit 1; fi; done

install release clean cleanall::
	for d in *; do if test -d $$d; then make -C $$d $@; fi; done

list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
