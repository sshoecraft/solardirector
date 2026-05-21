
all:
	./mklib.sh $(JS)

WPI=yes

include ../../Makefile.sd

install:
	./mklib.sh $@

clean:
	rm -rf .deps *.o *.so *.a
ifneq ($(_BUILD_BASE),)
	rm -rf $(OBJDIR)
endif

