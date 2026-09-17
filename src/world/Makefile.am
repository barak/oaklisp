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
ALLOAFILES += $(MAKEMAKEFILES)
ALLOAFILES += $(FILESFILES)
ALLOAFILES += $(COLDFILES)
ALLOAFILES += $(MISCFILES)
ALLOAFILES += $(COMPFILES)
ALLOAFILES += $(RNRSFILES)

ALLOAFILESNONGEN =
ALLOAFILESNONGEN += $(TOOLFILES)
ALLOAFILESNONGEN += $(MAKEMAKEFILES)
ALLOAFILESNONGEN += $(FILESFILES)
ALLOAFILESNONGEN += $(COLDFILESNONGEN)
ALLOAFILESNONGEN += $(MISCFILES)
ALLOAFILESNONGEN += $(COMPFILES)
ALLOAFILESNONGEN += $(RNRSFILES)

# Bootstrapping is set up by configure; see the comments there.
#
# $(OAK) runs the bootstrap system, an emulator with a working world,
# and is used to compile .oak sources to .oa bytecode and to link the
# cold world with tool.oak.  It is the "build" system in autotools
# terminology.  $(OAKBOOT) is the newly built emulator for the "host"
# system, used only for loading partial worlds, loading extra files
# into them, and dumping them.  When bootstrapping from prebuilt
# bytecode, $(OAK) is not needed at all.

OAK = $(OAKLISP) --batch
OAKBOOT = ../emulator/oaklisp --batch

# Flags for the emulator.
# OAKFLAGS = --trace-gc=1
OAKFLAGS =

# The architecture being built for.  OAK_BYTECODE_ARCH (e.g. bc2-64)
# is passed to the compiler and linkers; OAK_HOST_ARCH (e.g. bc2-el64)
# additionally names the byte order and is where a prebuilt world
# image for this machine lives.
OAK_HOST_ARCH = @OAK_HOST_ARCH@
OAK_BYTECODE_ARCH = @OAK_BYTECODE_ARCH@

# Where prebuilt material lives, and where "make prebuilt" puts it.
PREBUILT_DIR = $(top_srcdir)/prebuilt
PREBUILT_BYTECODE_DIR = @PREBUILT_BYTECODE_DIR@

if BOOTSTRAP_FROM_WORLD

# How to compile oaklisp source files into oaklisp bytecode object files.
# The compiler reads NAME.oak and writes NAME.oa in the current
# directory, so in a VPATH build the source is copied here first.
.oak.oa:
	@cmp -s $< $*.oak || cp $< $*.oak
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) -- $(OAKLOCALE) $(OAKTARGETFLAGS) \
	   --compile $* --exit

# How to build a new cold world using the world builder tool:
new.cold: $(COLDFILES) $(TOOLFILES)
	-rm -f new.cold new.sym
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) \
	   -- \
	   --load tool \
	   --eval "(tool-files '($(COLDFILESD:.oa=)) 'new \"$(OAK_BYTECODE_ARCH)\")" \
	   --exit

else

# No running Oaklisp: use the prebuilt bytecode ...
.oak.oa:
	cp $(PREBUILT_BYTECODE_DIR)/$@ $@

# ... except that the version string comes from configure.
system-version.oa: $(PREBUILT_BYTECODE_DIR)/system-version.oa
	$(SED) 's/"Oaklisp [^"]*"/"$(PACKAGE_STRING)"/' $(PREBUILT_BYTECODE_DIR)/system-version.oa > $@

# How to build a new cold world using the standalone C linker:
new.cold: $(COLDFILES)
	-rm -f new.cold new.sym
	../emulator/oak-cold-linker --target $(OAK_BYTECODE_ARCH) -o new \
	   $(COLDFILESD:.oa=)

endif

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

# Refresh the prebuilt material from the world just built.
#
# The bytecode is compiled for 32-bit fixnums (bc2-32) so that it can
# be used to bootstrap on any machine; the world image is only good
# for machines of the same byte order and word size.  Both go under
# prebuilt/src/world/ in subdirectories named by architecture.

prebuilt: prebuilt-bytecode prebuilt-world

prebuilt-bytecode: oakworld.bin
	rm -rf prebuilt-tmp
	$(MKDIR_P) prebuilt-tmp $(PREBUILT_DIR)/src/world/bc2-32
	cd $(srcdir) && cp $(ALLOAFILESNONGEN:.oa=.oak) $(abs_builddir)/prebuilt-tmp/
	cp system-version.oak prebuilt-tmp/
	cd prebuilt-tmp && for f in $(ALLOAFILES:.oa=); do \
	  case " $(COMPFILES:.oa=) " in \
	    *" $$f "*) loc="--locale compiler-locale";; \
	    *) loc="";; \
	  esac; \
	  $(abs_top_builddir)/src/emulator/oaklisp --batch $(OAKFLAGS) \
	     --world $(abs_builddir)/oakworld.bin \
	     -- $$loc --target bc2-32 --compile $$f --exit || exit 1; \
	done
	mv prebuilt-tmp/*.oa $(PREBUILT_DIR)/src/world/bc2-32/
	rm -rf prebuilt-tmp

prebuilt-world: oakworld.bin
	$(MKDIR_P) $(PREBUILT_DIR)/src/world/$(OAK_HOST_ARCH)
	cp oakworld.bin $(PREBUILT_DIR)/src/world/$(OAK_HOST_ARCH)/oakworld.bin

.PHONY: prebuilt prebuilt-bytecode prebuilt-world

# How to rebuild variables included by this Makefile itself.
# Makefile-vars is checked in; rebuild explicitly with: make Makefile-vars
# (No file prerequisites — otherwise make tries to build .oa files
# even during "make clean", which fails before the emulator is built.)

Makefile-vars:
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) \
	   -- \
	   --load files --load make-makefile --eval '(make-makefile "$@")' \
	   --exit

CLEANFILES = oakworld-1.bin oakworld-2.bin oakworld-3.bin new.cold	\
 new.sym $(ALLOAFILES) oakworld.bin

clean-local:
	rm -rf prebuilt-tmp

# Remove the .oak sources copied into a VPATH build directory.
distclean-local:
	if test "$(srcdir)" != .; then rm -f $(ALLOAFILESNONGEN:.oa=.oak); fi

EXTRA_DIST = $(ALLOAFILESNONGEN:.oa=.oak) $(GRAVY)
