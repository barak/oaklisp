# Copyright (c) 1999 Barak Pearlmutter
# Distributed under the GNU General Public License v2 or later

# This value of prefix will usually be overridden
prefix=/usr/local

psrc=oaklisp.c
osrc=cmdline.c data.c gc.c loop.c signal.c stacks.c timers.c weak.c worldio.c xmalloc.c instr.c threads.c
hsrc=cmdline.h config.h data.h gc.h loop.h signal.h stacks.h stacks-loop.h timers.h weak.h worldio.h xmalloc.h instr.h threads.h

progs=$(psrc:.c=)
pobj=$(psrc:.c=.o)
oobj=$(osrc:.c=.o)

all: $(progs) TAGS

CFLAGS=-O2 -g -Wall
LOADLIBES=-lpthread

OAK=oaklisp

# bootstrapping problem: to compile the emulator we need a working
# oaklisp to generate instr-data.c.

instr-data.c: instruction-table.oak
	$(OAK) $(OAKFLAGS) -- \
		--locale compiler-locale \
		--load instruction-table \
		--eval '(dump-instruction-table "$@")' \
		--exit
	-indent $@

# Things you might want to modify and uncomment:
# CPPFLAGS+=-DMAX_NEW_SPACE_SIZE=16000000
# CPPFLAGS+=-DDEFAULT_WORLD=\"/usr/share/lib/oaklisp/oakworld.bin\"
# CPPFLAGS+=-DFAST

CPPFLAGS+=-DDEFAULT_WORLD=\"$(prefix)/lib/oaklisp/oakworld.bin\"

$(progs): $(pobj) $(oobj)

# automatically update #include dependencies
dfiles = $(psrc:.c=.d) $(osrc:.c=.d)
-include $(dfiles)
override CPPFLAGS+=-MMD

TAGS: $(psrc) $(osrc) $(hsrc)
	etags $^ > $@

.PHONY: install
install: $(progs)
	cp -a $(progs) $(DESTDIR)$(prefix)/bin/
	cd $(DESTDIR)$(prefix)/bin; strip $(progs)

.PHONY: clean
clean:
	-rm -f $(progs) $(psrc:.c=.o) $(osrc:.c=.o) $(dfiles) TAGS

.PHONY: indent
indent: $(psrc) $(osrc) $(hsrc) 
	indent $^ --no-space-after-casts --no-space-after-function-call-names
