
all:
	@for d in *; do test "$$d" = "lib" && continue; if test -d $$d; then make -j $$MAKE_JOBS -C $$d || exit 1; fi; done

install release clean cleanall::
	for d in *; do if test -d $$d; then make -C $$d $@; fi; done

list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'
