# Copyright (c) 1999 Barak Pearlmutter
# Distributed under the GNU General Public License v2 or later

psrc=oakem.c
osrc=cmdline.c data.c gc.c instr.c loop.c signal.c stacks.c timers.c \
	weak.c worldio.c xmalloc.c
hsrc=cmdline.h config.h data.h gc.h loop.h signal.h stacks.h timers.h \
	weak.h worldio.h xmalloc.h

progs=$(psrc:.c=)
pobj=$(psrc:.c=.o)
oobj=$(osrc:.c=.o)

all: $(progs) TAGS

# automatically update #include dependencies
dfiles = $(psrc:.c=.d) $(msrc:.c=.d) $(osrc:.c=.d)
-include $(dfiles)
override CPPFLAGS+=-MMD

CFLAGS=-O2 -g -Wall

# Things you might want to modify and uncomment:
# CPPFLAGS+=-DMAX_NEW_SPACE_SIZE=16000000
# CPPFLAGS+=-DDEFAULT_WORLD=\"/usr/share/lib/oaklisp/oakworld.bin\"
# CPPFLAGS+=-DFAST

$(progs): $(pobj) $(oobj)

TAGS: $(psrc) $(osrc) $(hsrc)
	etags $^ > $@

.PHONY: install-nostrip
install-nostrip: $(progs)
	-rm -f ../bin/oakem
	cp -a $(progs) ../bin/

.PHONY: install
install: install-nostrip
	cd ../bin; strip $(progs)

.PHONY: clean
clean:
	-rm -f $(progs) $(psrc:.c=.o) $(osrc:.c=.o) $(dfiles) TAGS

.PHONY: release
release: $(psrc) $(osrc) $(hsrc) Makefile
	cp -a $^ $(RTARGET)/
