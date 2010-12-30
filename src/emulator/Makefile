# This file is part of Oaklisp.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# The GNU GPL is available at http://www.gnu.org/licenses/gpl.html
# or from the Free Software Foundation, 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA

# Copyright (c) 1999 Barak A. Pearlmutter
# Distributed under the GNU General Public License v2 or later

# This value of prefix will usually be overridden
prefix=/usr/local

psrc=oaklisp.c
osrc=cmdline.c data.c gc.c loop.c signal.c stacks.c timers.c weak.c worldio.c xmalloc.c instr.c threads.c
hsrc=cmdline.h config.h data.h gc.h loop.h signal.h stacks.h stacks-loop.h timers.h weak.h worldio.h xmalloc.h instr.h threads.h

progs=$(psrc:.c=)
pobj=$(psrc:.c=.o)
oobj=$(osrc:.c=.o)

all: $(progs)

CFLAGS += -O2 -g -Wall
LOADLIBES = -lpthread

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

d=$(DESTDIR)$(prefix)/bin

.PHONY: install
install: $(progs)
	mkdir --parents $d
	cp -a $(progs) $d/
	cd $d; strip $(progs)

.PHONY: clean
clean:
	-rm -f $(progs) $(psrc:.c=.o) $(osrc:.c=.o) $(dfiles) TAGS

.PHONY: indent
indent: $(psrc) $(osrc) $(hsrc)
	indent --no-space-after-casts --no-space-after-function-call-names $^
