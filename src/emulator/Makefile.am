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

# This Makefile needs GNU make (wildcard, firstword), like src/world's.
AUTOMAKE_OPTIONS = -Wno-portability

bin_PROGRAMS = oaklisp

# A C reimplementation of the cold linker tool.oak, kept as an
# independent check on it and for bootstrapping where nothing can run
# tool.oak; not built by default (configure --enable-cold-linker).
if COLD_LINKER
noinst_PROGRAMS = oak-cold-linker
endif
oak_cold_linker_SOURCES = oak-cold-linker.c
EXTRA_PROGRAMS = oak-cold-linker

# Everything but main() goes into liboakrt.a, so that a stand-alone
# executable built by oakc can link the emulator in under its own
# generated main (see oak-embed.h and oakc.in at the top level).
noinst_LIBRARIES = liboakrt.a
liboakrt_a_SOURCES = cmdline.c data.c dispatch.c embed-native.c gc.c	\
 gc-ms.c instr.c lineedit.c loop.c native.c native-ops.c oaklisp.c	\
 signals.c stacks.c threads.c timers.c weak.c worldio.c xmalloc.c	\
 cmdline.h config.h data.h dispatch.h gc.h gc-ms.h instr.h lineedit.h	\
 loop.h native.h native-ops.h oak-atomic.h oak-embed.h oak-getopt.h	\
 oak-header.h oak-native.h oak-thread.h signals.h stacks.h		\
 stacks-loop.h threads.h timers.h weak.h worldio.h xmalloc.h

oaklisp_SOURCES = main.c
oaklisp_LDADD = liboakrt.a $(LIBS)

# Natively compiled code is loaded with dlopen and links against the
# emulator itself.  It is compiled with the same compiler and flags,
# and includes oak-native.h from this tree (see native.c).
oaklisp_LDFLAGS = $(OAK_EXPORT_DYNAMIC)

if NDEBUG
else
BUILT_SOURCES = instr-data.c
endif

OAKRT_CPPFLAGS = $(AM_CPPFLAGS)

OAKRT_CPPFLAGS += -DDEFAULT_WORLD=\"$(pkglibdir)/oakworld.bin\"

if ENABLE_THREADS
OAKRT_CPPFLAGS += -DTHREADS -DUSE_MARK_SWEEP
endif

if NDEBUG
OAKRT_CPPFLAGS += -DFAST
OAK_NATIVE_FAST = -DFAST
endif

OAKRT_CPPFLAGS += -DOAK_NATIVE_CC='"$(CC)"'				\
 -DOAK_NATIVE_CFLAGS='"$(CFLAGS) -fPIC -fno-strict-aliasing"'		\
 -DOAK_NATIVE_SHARED='"@OAK_NATIVE_SHARED@"'				\
 -DOAK_NATIVE_INCLUDE='"-DHAVE_CONFIG_H $(OAK_NATIVE_FAST) -I$(abs_srcdir) -I$(abs_top_builddir)"'

liboakrt_a_CPPFLAGS = $(OAKRT_CPPFLAGS)
oaklisp_CPPFLAGS = $(OAKRT_CPPFLAGS)

# Things to consider as configure.ac options:
# OAKRT_CPPFLAGS += -DMAX_NEW_SPACE_SIZE=16000000

# bootstrapping problem: to compile the emulator we need a working
# oaklisp to generate instr-data.c.  This is solved by trying to
# generate it automatically if necessary (with the world just built if
# there is one, otherwise an old world as src/world would use), but if
# that fails instead using a prebuilt copy.  Note that when -DFAST is
# set instr-data.c is not used and this is not an issue; it is still
# generated for "make dist", so that tarballs can be built with
# --disable-ndebug without prebuilt/.

OAK_HOST_ARCH = @OAK_HOST_ARCH@
OAKLISP = ./oaklisp$(EXEEXT)
OLD_WORLD = $(firstword $(wildcard ../world/old-oakworld.bin \
	$(top_srcdir)/prebuilt/src/world/$(OAK_HOST_ARCH)/oakworld.bin) ../world/old-oakworld.bin)
OAKFLAGS =

instr-data.c: instruction-table.oak
	if test -x ./oaklisp$(EXEEXT) && test -f ../world/oakworld.bin; then \
	  oak='./oaklisp$(EXEEXT) --batch --world ../world/oakworld.bin'; \
	else \
	  oak='$(OAKLISP) --batch $(OAKFLAGS) --world $(OLD_WORLD)'; \
	fi; \
	$$oak -- \
		--locale compiler-locale \
		--load "$<" \
		--eval '(dump-instruction-table "$@")' \
		--exit \
	|| cp $(top_srcdir)/prebuilt/src/emulator/instr-data.c $@
	-$(INDENT) $@

# Refresh the prebuilt copy using the world just built.
prebuilt:
	$(MKDIR_P) $(top_srcdir)/prebuilt/src/emulator
	./oaklisp --batch --world ../world/oakworld.bin -- \
		--locale compiler-locale \
		--load $(srcdir)/instruction-table.oak \
		--eval '(dump-instruction-table "$(top_srcdir)/prebuilt/src/emulator/instr-data.c")' \
		--exit
	-$(INDENT) $(top_srcdir)/prebuilt/src/emulator/instr-data.c

.PHONY: prebuilt

EXTRA_DIST = instruction-table.oak instr-data.c

CLEANFILES = instr-data.c
