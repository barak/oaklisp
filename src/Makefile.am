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

SUBDIRS = . world

bin_PROGRAMS = oaklisp

oaklisp_SOURCES = emulator/cmdline.c emulator/data.c emulator/gc.c	\
 emulator/instr.c emulator/instr-data.c emulator/loop.c			\
 emulator/oaklisp.c emulator/signals.c emulator/stacks.c		\
 emulator/threads.c emulator/timers.c emulator/weak.c			\
 emulator/worldio.c emulator/xmalloc.c emulator/cmdline.h		\
 emulator/config.h emulator/data.h emulator/gc.h emulator/instr.h	\
 emulator/loop.h emulator/signals.h emulator/stacks.h			\
 emulator/stacks-loop.h emulator/threads.h emulator/timers.h		\
 emulator/weak.h emulator/worldio.h emulator/xmalloc.h

oaklisp_LDADD = -lpthread

# Things you might want to modify and uncomment:
# CPPFLAGS+=-DMAX_NEW_SPACE_SIZE=16000000
# CPPFLAGS+=-DDEFAULT_WORLD=\"/usr/share/lib/oaklisp/oakworld.bin\"
# CPPFLAGS+=-DFAST
# CPPFLAGS+=-DTHREADS

oaklisp_CPPFLAGS = \
 -Iemulator \
 -DFAST \
 -DDEFAULT_WORLD=\"$(pkglibdir)/oakworld.bin\"

if USE_M32
oaklisp_CFLAGS = -m32
oaklisp_LDFLAGS = -m32
endif

# bootstrapping problem: to compile the emulator we need a working
# oaklisp to generate instr-data.c.  This is solved by trying to
# generate it automatically if necessary, but if that fails instead
# using a prebuilt copy.

OAK=oaklisp

emulator/instr-data.c: emulator/instruction-table.oak
	$(OAK) $(OAKFLAGS) -- \
		--locale compiler-locale \
		--load "$<" \
		--eval '(dump-instruction-table "$@")' \
		--exit \
	|| cp ../prebuilt/src/emulator/instr-data.c $@
	-$(INDENT) $@

EXTRA_DIST = emulator/instruction-table.oak emulator/instr-data.c	\
 misc/README misc/testing-tests.oak misc/uniq.oak			\
 misc/unit-testing.oak

CLEANFILES = emulator/instr-data.c
