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

# Bigendian machines need bigendian binary oaklisp worlds, so non-cold
# worlds are only compatible across machines of the same endianity.

workaroundpkglibdir=$(pkglibdir)
workaroundpkglib_DATA = oakworld.bin

include Makefile-vars

ALLOAFILES =
ALLOAFILES += $(TOOLFILES)
ALLOAFILES += $(MAKEFILES)
ALLOAFILES += $(FILESFILES)
ALLOAFILES += $(COLDFILES)
ALLOAFILES += $(MISCFILES)
ALLOAFILES += $(COMPFILES)
ALLOAFILES += $(RNRSFILES)

ALLOAFILESNONGEN =
ALLOAFILESNONGEN += $(TOOLFILES)
ALLOAFILESNONGEN += $(MAKEFILES)
ALLOAFILESNONGEN += $(FILESFILES)
ALLOAFILESNONGEN += $(COLDFILESNONGEN)
ALLOAFILESNONGEN += $(MISCFILES)
ALLOAFILESNONGEN += $(COMPFILES)
ALLOAFILESNONGEN += $(RNRSFILES)

# The Oaklisp emulator executable.
# Use "OAK=oaklisp" unless compiling new emulator.
OAK = ../emulator/oaklisp

# When cross-compiling, $(OAK) is for an emulator with a working
# oakworld.bin, the "build" system in autotools terminology, while
# $(OAKBOOT) is for the new architecture, the "host" system in
# autotools terminology.  $(OAKBOOT) will be used only for loading
# partial worlds, loading extra files into them, and dumping them.
OAKBOOT = ../emulator/oaklisp

# Flags for the emulator.
# OAKFLAGS = --trace-gc=1
OAKFLAGS =

# When bootstrapping add "OAKFLAGS+=--world ...../oakworld.bin"
# generally: ../../prebuilt/src/world/oakworld.bin

# ifeq ($(shell $(OAK) $(OAKFLAGS) -- --exit > /dev/null || echo oops),oops)
# OAKWORLDFLAGS = --world  ../../prebuilt/src/world/oakworld.bin
# endif

# How to compile oaklisp source files into oaklisp bytecode object files:
.oak.oa:
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) -- $(OAKLOCALE) --compile $* --exit

# How to build a new cold world using the world builder tool:
new.cold: $(COLDFILES) $(TOOLFILES)
	-rm -f new.cold new.sym
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) \
	   -- \
	   --load tool \
	   --eval "(tool-files '($(COLDFILESD:.oa=)) 'new)" \
	   --exit

# How to boot cold world into warm world:
oakworld-1.bin: new.cold
	 $(OAKBOOT) $(OAKFLAGS) --world new.cold --dump $@

# load successive layers of functionality onto the world

oakworld-2.bin: oakworld-1.bin $(MISCFILES)
	$(OAKBOOT) $(OAKFLAGS) --world oakworld-1.bin --dump $@ \
	   -- \
	   $(foreach f, $(MISCFILES:.oa=),--load $(f)) \
	   --exit

oakworld-3.bin: oakworld-2.bin $(COMPFILES)
	 $(OAKBOOT) $(OAKFLAGS) --world oakworld-2.bin --dump $@ \
	   -- \
	   --locale compiler-locale \
	   $(foreach f, $(COMPFILES:.oa=),--load $(f)) \
	   --locale system-locale --exit

oakworld.bin: oakworld-3.bin $(RNRSFILES)
	$(OAKBOOT) $(OAKFLAGS) --world oakworld-3.bin --dump $@ \
	  -- \
	  --eval '(define-instance scheme-locale locale (list system-locale))'\
	  --locale scheme-locale \
	  $(foreach f, $(RNRSFILES:.oa=),--load $(f)) \
	  --locale system-locale --exit

# How to rebuild variables included by this Makefile itself:

Makefile-vars: $(MAKEFILES) $(FILESFILES)
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) \
	   -- \
	   --load files --load make-makefile --eval '(make-makefile "$@")' \
	   --exit

CLEANFILES = oakworld-1.bin oakworld-2.bin oakworld-3.bin new.cold	\
 new.sym $(ALLOAFILES) oakworld.bin

EXTRA_DIST = $(ALLOAFILESNONGEN:.oa=.oak) $(GRAVY)
