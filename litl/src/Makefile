include ../Makefile.config

LDFLAGS=-L../obj/CLHT/external/lib -L../obj/CLHT -L../../ -L../../.output/ -Wl,--whole-archive -Wl,--version-script=interpose.map -lsync -lsspfd -lssmem -lclht -Wl,--no-whole-archive  -lrt -lm -ldl -lpapi -m64 -pthread -lelf -lz
CFLAGS=-I../include/ -I../obj/CLHT/include/ -I../obj/CLHT/external/include/include/ $(EXTERNAL_CFLAGS) -fPIC -Wall -Werror -O3 -g -D_GNU_SOURCE

# Keep objects files
.PRECIOUS: %.o
.SECONDARY: $(OBJS)

.SECONDEXPANSION:
../obj/%.o: $$(lastword $$(subst /, ,%)).c $$(lastword $$(subst /, ,%)).h
	$(eval $@_TMP := $(shell echo $@ | cut -d/ -f3 | cut -d_ -f1))
	$(CC) $(CFLAGS) -D$$(echo $@ | cut -d/ -f3 | cut -d_ -f1 | tr '[a-z]' '[A-Z]') -DCOND_VAR=$(COND_VAR) -DFCT_LINK_SUFFIX=$($@_TMP) -DWAITING_$$(echo $@ | cut -d/ -f3 | cut -d_ -f2- | tr '[a-z]' '[A-Z]') -o $@ -c $<

.SECONDEXPANSION:
../obj/%.o: $$(firstword $$(subst _, , $$(lastword $$(subst /, ,%)))).c ../include/$$(firstword $$(subst _, , $$(lastword $$(subst /, ,%)))).h
	$(eval $@_TMP := $(shell echo $@ | cut -d/ -f3 | cut -d_ -f1))
	$(CC) $(CFLAGS) -D$$(echo $@ | cut -d/ -f3 | cut -d_ -f1 | tr '[a-z]' '[A-Z]') -DCOND_VAR=$(COND_VAR) -DFCT_LINK_SUFFIX=$($@_TMP) -DWAITING_$$(echo $@ | cut -d/ -f3 | cut -d_ -f2- | tr '[a-z]' '[A-Z]') -o $@ -c $<

.SECONDEXPANSION:
../lib/lib%.so: ../obj/%/interpose.o ../obj/%/utils.o $$(subst algo,%,../obj/algo/algo.o) $(EXTERNAL_OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
