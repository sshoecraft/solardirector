
all: mkh mkob

mkh: mkh.c
	$(FULLCC) -o mkh mkh.c $(LIBS)

mkob: mkob.c sma_objects.c sma_strings.c $(DEPS)
	$(FULLCC) -o $@ $< $(LIBS)

include ../../../Makefile.sd
