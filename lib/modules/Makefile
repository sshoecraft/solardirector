
MODULES=test

all: $(MODULES)

#$(SLIB): $(SOBJS)
##       $(CC) $(LDADD) $(SHLDADD) -shared -o $(SLIB) $(SOBJS) $(SHAREDLIBS)
#        $(FULLCC) $(LDADD) $(SHLDADD) -shared -o $(SLIB) $(SOBJS) $(SHAREDLIBS)

test: test.c
	gcc -Fpic -shared -o test.so test.c

include ../../Makefile.sd
