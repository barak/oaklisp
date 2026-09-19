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
ALLOAFILES += $(EXPORTFILES)
ALLOAFILES += $(RNRSFILES)

ALLOAFILESNONGEN =
ALLOAFILESNONGEN += $(TOOLFILES)
ALLOAFILESNONGEN += $(MAKEMAKEFILES)
ALLOAFILESNONGEN += $(FILESFILES)
ALLOAFILESNONGEN += $(COLDFILESNONGEN)
ALLOAFILESNONGEN += $(MISCFILES)
ALLOAFILESNONGEN += $(COMPFILES)
ALLOAFILESNONGEN += $(EXPORTFILES)
ALLOAFILESNONGEN += $(RNRSFILES)

# Bootstrapping is set up by configure; see the comments there.
#
# Two steps need a running Oaklisp (or a stand-in for one), and each
# can be done in several ways, chosen independently by configure:
#
#   compiling .oak sources to .oa bytecode    COMPILE_WORLD:   $(OAK), the
#                                                bootstrap emulator and world
#                                             COMPILE_PREBUILT: copy from
#                                                $(PREBUILT_BYTECODE_DIR)
#                                             COMPILE_GUILE:   the Guile
#                                                hosted compiler
#
#   linking the cold files into new.cold      COLD_LINK_WORLD: tool.oak in
#                                                $(OAK)
#                                             COLD_LINK_C:     oak-cold-linker
#                                             COLD_LINK_GUILE: tool.oak in
#                                                the Guile host
#
# $(OAK) runs the bootstrap system, an emulator with a working world.
# It is the "build" system in autotools terminology.  $(OAKBOOT) is
# the newly built emulator for the "host" system, used to boot the cold
# world, load the remaining layers into it, and dump the result.

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

# The Guile-hosted Oaklisp (src/cold-compiler): it loads the world's
# own macros, compiler and linker from their .oak sources and runs
# them, so its objects are byte for byte what a native Oaklisp would
# produce.  Building its world takes some seconds, so it compiles
# everything in one run.
OAKGUILE = $(GUILE) --no-auto-compile \
  $(top_srcdir)/src/cold-compiler/oak-bootstrap.scm --srcdir $(srcdir) \
  --target $(OAK_BYTECODE_ARCH) --noisy 0
GUILEHOST = $(top_srcdir)/src/cold-compiler/oak-bootstrap.scm	\
  $(top_srcdir)/src/cold-compiler/host/reader.scm			\
  $(top_srcdir)/src/cold-compiler/host/kernel.scm			\
  $(top_srcdir)/src/cold-compiler/host/translate.scm			\
  $(top_srcdir)/src/cold-compiler/host/macros.scm			\
  $(top_srcdir)/src/cold-compiler/host/runtime.scm			\
  $(top_srcdir)/src/cold-compiler/host/runtime2.scm			\
  $(top_srcdir)/src/cold-compiler/host/world.scm
GUILEOBJS = $(ALLOAFILES)
GUILESRCS = $(COLDFILESNONGEN:.oa=) ./system-version $(MISCFILES:.oa=)	\
  $(EXPORTFILES:.oa=) $(TOOLFILES:.oa=) $(FILESFILES:.oa=)		\
  $(MAKEMAKEFILES:.oa=) --locale compiler-locale $(COMPFILES:.oa=)	\
  --locale scheme-locale $(RNRSFILES:.oa=)

##### Compiling

if COMPILE_WORLD

# How to compile oaklisp source files into oaklisp bytecode object files.
# The compiler reads NAME.oak and writes NAME.oa in the current
# directory, so in a VPATH build the source is copied here first.
#
# The bootstrap world may be older than the sources: its assembler may
# not know an instruction or register the sources use, and a call to
# an open coded operation it does not know would be compiled as an
# ordinary call, which such an operation cannot answer.  So the
# assembler is compiled first and loaded into the bootstrap world's
# compiler before anything else is compiled, and the few files that
# use new open coded operations from other files get those loaded
# too.  (A current bootstrap world compiles everything the same
# either way.)
OAKPRELOAD = --locale compiler-locale --load assembler --locale system-locale

%.oa: %.oak assembler.oa
	@cmp -s $< $*.oak || cp $< $*.oak
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) -- $(OAKPRELOAD) $(OAKLOCALE) \
	   $(OAKTARGETFLAGS) --compile $* --exit

assembler.oa: assembler.oak
	@cmp -s $< $*.oak || cp $< $*.oak
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) -- --locale compiler-locale \
	   $(OAKTARGETFLAGS) --compile $* --exit

# multiproc.oak uses the %THREADS-ENABLED? operation multi-em.oak
# defines, and file-io.oak's RENAME-FILE uses (%STREAM-PRIMITIVE 14)
# from streams.oak.
multiproc.oa: multiproc.oak multi-em.oa assembler.oa
	@cmp -s $< $*.oak || cp $< $*.oak
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) -- $(OAKPRELOAD) --load multi-em \
	   $(OAKTARGETFLAGS) --compile $* --exit

file-io.oa: file-io.oak streams.oa assembler.oa
	@cmp -s $< $*.oak || cp $< $*.oak
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) -- $(OAKPRELOAD) --load streams \
	   $(OAKTARGETFLAGS) --compile $* --exit

endif

if COMPILE_PREBUILT

# No running Oaklisp: use the prebuilt bytecode ...
.oak.oa:
	cp $(PREBUILT_BYTECODE_DIR)/$@ $@

# ... except that the version string comes from configure.
system-version.oa: $(PREBUILT_BYTECODE_DIR)/system-version.oa
	$(SED) 's/"Oaklisp [^"]*"/"$(PACKAGE_STRING)"/' $(PREBUILT_BYTECODE_DIR)/system-version.oa > $@

endif

if COMPILE_GUILE

# Source only: no prebuilt objects and no existing Oaklisp are used.
# system-version.oak is generated by configure into the build directory,
# hence the ./ that keeps the compiler from looking for it in srcdir.
guile.stamp: $(ALLOAFILESNONGEN:.oa=.oak) system-version.oak $(GUILEHOST)
	$(OAKGUILE) --outdir . $(GUILESRCS)
	touch $@

$(GUILEOBJS): guile.stamp
	@test -f $@ || { rm -f guile.stamp; $(MAKE) $(AM_MAKEFLAGS) guile.stamp; }

endif

##### Linking

if COLD_LINK_WORLD

# How to build a new cold world using the world builder tool:
new.cold: $(COLDFILES) $(TOOLFILES)
	-rm -f new.cold new.sym
	$(OAK) $(OAKFLAGS) $(OAKWORLDFLAGS) \
	   -- \
	   --load tool \
	   --eval "(tool-files '($(COLDFILESD:.oa=)) 'new \"$(OAK_BYTECODE_ARCH)\")" \
	   --exit

endif

if COLD_LINK_C

# How to build a new cold world using the standalone C linker:
new.cold: $(COLDFILES)
	-rm -f new.cold new.sym
	../emulator/oak-cold-linker --target $(OAK_BYTECODE_ARCH) -o new \
	   $(COLDFILESD:.oa=)

endif

if COLD_LINK_GUILE

# How to build a new cold world using the world builder tool in the
# Guile host:
new.cold: $(COLDFILES) $(GUILEHOST)
	-rm -f new.cold new.sym
	$(OAKGUILE) --load tool \
	   --eval "(tool-files '($(COLDFILESD:.oa=)) \"new\" \"$(OAK_BYTECODE_ARCH)\")"

endif

##### Booting

# How to boot cold world into warm world:
oakworld-1.bin: new.cold
	 $(OAKBOOT) $(OAKFLAGS) --world new.cold --dump $@

# load successive layers of functionality onto the world

oakworld-2.bin: oakworld-1.bin $(MISCFILES)
	$(OAKBOOT) $(OAKFLAGS) --world oakworld-1.bin --dump $@ \
	   -- \
	   $(foreach f, $(MISCFILES:.oa=),--load $(f)) \
	   --exit

oakworld-3.bin: oakworld-2.bin $(COMPFILES) $(EXPORTFILES)
	 $(OAKBOOT) $(OAKFLAGS) --world oakworld-2.bin --dump $@ \
	   -- \
	   --locale compiler-locale \
	   $(foreach f, $(COMPFILES:.oa=),--load $(f)) \
	   --locale system-locale \
	   $(foreach f, $(EXPORTFILES:.oa=),--load $(f)) \
	   --exit

oakworld.bin: oakworld-3.bin $(RNRSFILES)
	$(OAKBOOT) $(OAKFLAGS) --world oakworld-3.bin --dump $@ \
	  -- \
	  --eval '(define-instance scheme-locale locale (list system-locale))'\
	  --eval "(export-symbol system-locale oaklisp-locale 'scheme-locale)" \
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
	  loc=""; \
	  case " $(COMPFILES:.oa=) " in *" $$f "*) loc="--locale compiler-locale";; esac; \
	  case " $(RNRSFILES:.oa=) " in *" $$f "*) loc="--locale scheme-locale";; esac; \
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

##### Consistency checks (run by "make check" at the top level)

# Every way of producing an object or a cold world should agree with
# every other, up to the architecture header line.  These checks each
# need a working oakworld.bin, so run them after a full build.

# The world just built must compile its own sources to exactly the
# objects it was built from (the compiler is deterministic and has
# reached a fixpoint).  Only the header line may differ, as the
# objects may have been compiled for another word size.
check-fixpoint: oakworld.bin $(ALLOAFILES)
	@rm -rf fixpoint-check && $(MKDIR_P) fixpoint-check
	@cd $(srcdir) && cp $(ALLOAFILESNONGEN:.oa=.oak) $(abs_builddir)/fixpoint-check/
	@cp system-version.oak fixpoint-check/
	@rc=0; cd fixpoint-check && for f in $(ALLOAFILES:.oa=); do \
	  cmpbody() { tail -n +2 "$$1" > cmp1.tmp; tail -n +2 "$$2" > cmp2.tmp; cmp -s cmp1.tmp cmp2.tmp; }; \
	  loc=""; \
	  case " $(COMPFILES:.oa=) " in *" $$f "*) loc="--locale compiler-locale";; esac; \
	  case " $(RNRSFILES:.oa=) " in *" $$f "*) loc="--locale scheme-locale";; esac; \
	  $(abs_top_builddir)/src/emulator/oaklisp --batch $(OAKFLAGS) \
	    --world $(abs_builddir)/oakworld.bin -- $$loc --compile $$f --exit \
	    > $$f.log 2>&1 || { echo "compile failed: $$f (see fixpoint-check/$$f.log)"; rc=1; continue; }; \
	  cmpbody $$f.oa ../$$f.oa \
	    || { echo "$$f.oa: recompiled object differs from the one built from"; rc=1; }; \
	done; \
	test $$rc = 0 && echo "check-fixpoint: the world reproduces its own objects."; \
	exit $$rc

# The Guile-hosted compiler must produce the same objects as the
# native one.  Needs Guile.
check-guile-compile: oakworld.bin $(ALLOAFILES)
	@test x$(GUILE) != xno || { echo "check-guile-compile: no Guile; skipped"; exit 0; }
	@rm -rf guile-check && $(MKDIR_P) guile-check
	@$(OAKGUILE) --outdir guile-check $(GUILESRCS) > guile-check/log 2>&1 \
	  || { echo "Guile-hosted compile failed; see guile-check/log"; exit 1; }
	@rc=0; cd guile-check && for f in $(ALLOAFILES); do \
	  cmpbody() { tail -n +2 "$$1" > cmp1.tmp; tail -n +2 "$$2" > cmp2.tmp; cmp -s cmp1.tmp cmp2.tmp; }; \
	  cmpbody $$f ../$$f \
	    || { echo "$$f: Guile-hosted compiler's object differs"; rc=1; }; \
	done; \
	test $$rc = 0 && echo "check-guile-compile: the Guile-hosted compiler reproduces the objects."; \
	exit $$rc

# tool.oak (in the world just built), oak-cold-linker, and tool.oak in
# the Guile host must produce identical cold worlds from the same
# objects.
check-cold-linkers: oakworld.bin new.cold $(TOOLFILES)
	@rm -f linkcheck-*.cold linkcheck-*.sym
	@../emulator/oak-cold-linker --target $(OAK_BYTECODE_ARCH) -o linkcheck-c \
	   $(COLDFILESD:.oa=) > /dev/null 2>&1 \
	  || { echo "oak-cold-linker failed"; exit 1; }
	@$(OAKBOOT) $(OAKFLAGS) --world oakworld.bin -- --load tool \
	  --eval "(tool-files '($(COLDFILESD:.oa=)) 'linkcheck-oak \"$(OAK_BYTECODE_ARCH)\")" \
	  --exit > /dev/null 2>&1 \
	  || { echo "tool.oak cold link failed"; exit 1; }
	@rc=0; cmp -s linkcheck-c.cold linkcheck-oak.cold \
	  || { echo "oak-cold-linker and tool.oak produce different cold worlds"; rc=1; }; \
	if test x$(GUILE) != xno; then \
	  $(OAKGUILE) --load tool \
	    --eval "(tool-files '($(COLDFILESD:.oa=)) \"linkcheck-guile\" \"$(OAK_BYTECODE_ARCH)\")" \
	    > /dev/null 2>&1 || { echo "tool.oak under Guile failed"; rc=1; }; \
	  cmp -s linkcheck-guile.cold linkcheck-oak.cold \
	    || { echo "tool.oak under Guile produces a different cold world"; rc=1; }; \
	fi; \
	test $$rc = 0 && echo "check-cold-linkers: all cold linkers agree."; \
	rm -f linkcheck-*.cold linkcheck-*.sym; \
	exit $$rc

.PHONY: check-fixpoint check-guile-compile check-cold-linkers

CLEANFILES = oakworld-1.bin oakworld-2.bin oakworld-3.bin new.cold	\
 new.sym $(ALLOAFILES) oakworld.bin guile.stamp

clean-local:
	rm -rf prebuilt-tmp fixpoint-check guile-check

# Remove the .oak sources copied into a VPATH build directory.
distclean-local:
	if test "$(srcdir)" != .; then rm -f $(ALLOAFILESNONGEN:.oa=.oak); fi

EXTRA_DIST = $(ALLOAFILESNONGEN:.oa=.oak) $(GRAVY)
