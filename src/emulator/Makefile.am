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

bin_PROGRAMS = oaklisp

oaklisp_SOURCES = cmdline.c data.c gc.c instr.c loop.c oaklisp.c	\
 signals.c stacks.c threads.c timers.c weak.c worldio.c xmalloc.c	\
 cmdline.h config.h data.h gc.h instr.h loop.h signals.h stacks.h	\
 stacks-loop.h threads.h timers.h weak.h worldio.h xmalloc.h

if NDEBUG
else
BUILT_SOURCES = instr-data.c
endif

oaklisp_CPPFLAGS = -DDEFAULT_WORLD=\"$(pkglibdir)/oakworld.bin\"

if ENABLE_THREADS
oaklisp_CPPFLAGS += -DTHREADS
endif

if NDEBUG
oaklisp_CPPFLAGS += -DFAST
endif

# Things to consider as configure.ac options:
# oaklisp_CPPFLAGS += -DMAX_NEW_SPACE_SIZE=16000000

# bootstrapping problem: to compile the emulator we need a working
# oaklisp to generate instr-data.c.  This is solved by trying to
# generate it automatically if necessary, but if that fails instead
# using a prebuilt copy.  Note that when -DFAST is set instr-data.c is
# not used and this is not an issue.

OAK=oaklisp

instr-data.c: instruction-table.oak
	$(OAK) $(OAKFLAGS) -- \
		--locale compiler-locale \
		--load "$<" \
		--eval '(dump-instruction-table "$@")' \
		--exit \
	|| cp ../prebuilt/src/emulator/instr-data.c $@
	-$(INDENT) $@

EXTRA_DIST = instruction-table.oak instr-data.c

CLEANFILES = instr-data.c
