# Copyright (c) 1999 Barak Pearlmutter
# Distributed under the GNU General Public License v2 or later

psrc=oakem.c
osrc=cmdline.c data.c gc.c loop.c signal.c stacks.c timers.c \
	weak.c worldio.c xmalloc.c instr.c threads.c
hsrc=cmdline.h config.h data.h gc.h loop.h signal.h stacks.h timers.h \
	weak.h worldio.h xmalloc.h instr.h threads.h

progs=$(psrc:.c=)
pobj=$(psrc:.c=.o)
oobj=$(osrc:.c=.o)

all: $(progs) TAGS

# automatically update #include dependencies
dfiles = $(psrc:.c=.d) $(osrc:.c=.d)
-include $(dfiles)
override CPPFLAGS+=-MMD

CFLAGS=-O2 -g -Wall
LDFLAGS=-lpthread

OAK=oakem

#We have a small "bootstrapping" problem here.  In order to compile oakem,
#we need oakem to make instr-data.c.  So let's save the last one.  If we
#fail to update, oh well.  Just finish and try again.
instr-data.c: instruction-table.oak ../world/assembler.oak ../lib/oakworld.bin
	-$(OAK) $(OAKFLAGS) -- \
		-locale compiler-locale -load instruction-table \
		-eval '(dump-instruction-table "$@")' \
		-exit
	-indent $@

# Things you might want to modify and uncomment:
# CPPFLAGS+=-DMAX_NEW_SPACE_SIZE=16000000
# CPPFLAGS+=-DDEFAULT_WORLD=\"/usr/share/lib/oaklisp/oakworld.bin\"
# CPPFLAGS+=-DFAST

CFLAGS+=-DDEFAULT_WORLD=\"${OAKDIR}/lib/oakworld.bin\"

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
release: $(psrc) $(osrc) $(hsrc) Makefile instruction-table.oak
	cp -a $^ $(RTARGET)/

.PHONY: indent
indent: $(psrc) $(osrc) $(hsrc) 
	indent $^ --no-space-after-casts --no-space-after-function-call-names
