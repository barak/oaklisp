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
# Use "OAK=oaklisp --batch" unless compiling new emulator.
OAK = ../emulator/oaklisp --batch

# When cross-compiling, $(OAK) is for an emulator with a working
# oakworld.bin, the "build" system in autotools terminology, while
# $(OAKBOOT) is for the new architecture, the "host" system in
# autotools terminology.  $(OAKBOOT) will be used only for loading
# partial worlds, loading extra files into them, and dumping them.
OAKBOOT = ../emulator/oaklisp --batch

# Flags for the emulator.
# OAKFLAGS = --trace-gc=1
OAKFLAGS =

# When bootstrapping add "OAKFLAGS+=--world ...../oakworld.bin"
# generally: ../../prebuilt/src/world/oakworld.bin

# ifeq ($(shell $(OAK) $(OAKFLAGS) -- --exit > /dev/null || echo oops),oops)
# OAKWORLDFLAGS = --world  ../../prebuilt/src/world/oakworld.bin
# endif

# How to compile oaklisp source files into oaklisp bytecode object files:
# If compilation fails (no working oaklisp), fall back to prebuilt .oa copy.
.oak.oa:
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) -- $(OAKLOCALE) --compile $* --exit \
	|| cp $(srcdir)/../../prebuilt/src/world/$@ $@

# Cold files come from prebuilt/ rather than being compiled here, so
# that a tree with no working Oaklisp can still be built.  To update a
# prebuilt .oa after modifying a cold .oak source, build a working world
# first and recompile with it.  Recompiling an unmodified cold source
# reproduces its prebuilt object byte for byte; if it does not, the
# compiler and the prebuilt objects have drifted apart and the
# regenerated object should not be shipped without checking why.
$(COLDFILES): %.oa: %.oak
	cp $(srcdir)/../../prebuilt/src/world/$@ $@

# multiproc.oa (a MISCFILE) depends on assembler.oa (register-list with
# threads-enabled?) and multi-em.oa (%threads-enabled? open-coded
# operation).  Without these loaded first, the compiler generates wrong
# bytecode for thread primitives.
multiproc.oa: multiproc.oak multi-em.oa assembler.oa
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) -- \
	  --locale compiler-locale --load assembler --locale system-locale \
	  --load multi-em --compile multiproc --exit \
	|| cp $(srcdir)/../../prebuilt/src/world/$@ $@

# When building for 64-bit, tell oak-cold-linker to emit 64-bit refs.
if BITS64
COLD_LINKER_FLAGS = --64bit
else
COLD_LINKER_FLAGS = --32bit
endif

# How to build a new cold world using oak-cold-linker:
new.cold: $(COLDFILES)
	-rm -f new.cold new.sym
	../emulator/oak-cold-linker $(COLD_LINKER_FLAGS) -o new $(COLDFILESD:.oa=)

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

# How to rebuild variables included by this Makefile itself.
# Makefile-vars is checked in; rebuild explicitly with: make Makefile-vars
# (No file prerequisites — otherwise make tries to build .oa files
# even during "make clean", which fails before the emulator is built.)

Makefile-vars:
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) \
	   -- \
	   --load files --load make-makefile --eval '(make-makefile "$@")' \
	   --exit

# Release check: recompile every source with the world that was just
# built and compare the result against the object shipped in prebuilt/.
# A difference means the sources and the shipped world have drifted
# apart, which is how several bugs came to live in objects that could
# not be seen by reading the sources.  This recompiles in place, so the
# objects here are left as the freshly compiled ones; they should be
# identical, and if they are not the check says which.
#
# Needs a working oakworld.bin, so run it after a full build.

check-prebuilt: oakworld.bin
	@rc=0; \
	for f in $(ALLOAFILESNONGEN); do \
	  b=`echo $$f | sed 's/\.oa$$//'`; \
	  loc=""; \
	  case " $(COMPFILES) " in *" $$f "*) loc="--locale compiler-locale";; esac; \
	  if test "$$f" = "multiproc.oa"; then \
	    $(OAK) $(OAKFLAGS) --world oakworld.bin -- \
	      --locale compiler-locale --load assembler --locale system-locale \
	      --load multi-em --compile $$b --exit > /dev/null 2>&1 \
	      || { echo "compile failed: $$b"; rc=1; continue; }; \
	  else \
	    $(OAK) $(OAKFLAGS) --world oakworld.bin -- $$loc --compile $$b --exit \
	      > /dev/null 2>&1 || { echo "compile failed: $$b"; rc=1; continue; }; \
	  fi; \
	  if test -f $(srcdir)/../../prebuilt/src/world/$$f; then \
	    cmp -s $$f $(srcdir)/../../prebuilt/src/world/$$f \
	      || { echo "$$f differs from prebuilt/src/world/$$f"; rc=1; }; \
	  fi; \
	done; \
	test $$rc = 0 && echo "All objects reproduce prebuilt/src/world/."; \
	exit $$rc

# Release check: oak-cold-linker is a C reimplementation of tool.oak, so
# the two must produce the same cold world from the same objects.  Both
# lay symbols out in the order they first see them rather than in hash
# table order, which is what makes that comparison exact rather than
# merely equivalent.
#
# Needs a working oakworld.bin, so run it after a full build.

check-cold-linker: oakworld.bin new.cold
	@rm -f tool-cold.cold tool-cold.sym
	@$(OAK) $(OAKFLAGS) --world oakworld.bin -- \
	  --load files --load tool \
	  --eval '(tool-files (quote ($(COLDFILESD:.oa=))) "tool-cold")' \
	  --exit > /dev/null 2>&1 \
	  || { echo "tool.oak cold link failed"; rm -f tool-cold.*; exit 1; }
	@cmp new.cold tool-cold.cold \
	  && echo "oak-cold-linker reproduces tool.oak's cold world." \
	  || { echo "new.cold differs from tool.oak's cold world."; \
	       rm -f tool-cold.*; exit 1; }
	@rm -f tool-cold.cold tool-cold.sym

CLEANFILES = oakworld-1.bin oakworld-2.bin oakworld-3.bin new.cold	\
 new.sym tool-cold.cold tool-cold.sym $(ALLOAFILES) oakworld.bin

EXTRA_DIST = $(ALLOAFILESNONGEN:.oa=.oak) $(GRAVY)
