
MAKE_JOBS?=$(shell cat /proc/cpuinfo | grep -c ^processor)

all:
#	for d in sd smanet js transports bt; do $(MAKE) -j $(MAKE_JOBS) -C $$d; done
	@$(MAKE) -C lib
	@for d in *; do if test -f $$d/Makefile; then $(MAKE) -C $$d || exit 1; fi; done

install release::
	for d in *; do if test -f $$d/Makefile; then $(MAKE) -C $$d $@; fi; done
	systemctl daemon-reload

clean:
	for d in *; do if test -f $$d/Makefile; then $(MAKE) -C $$d $@; fi; done
	find . -name "*.a" | xargs rm -f
	find . -name "*.so" | xargs rm -f

list:
	@$(MAKE) -pRrq -f $(lastword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/^# File/,/^# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | egrep -v -e '^[^[:alnum:]]' -e '^$@$$'


push: clean
	git add -A .
	git commit -m update
	git push
