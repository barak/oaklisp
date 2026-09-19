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

# This Makefile needs GNU make (pattern rules, foreach, filter-out).
AUTOMAKE_OPTIONS = -Wno-portability

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

# Bootstrapping.  Oaklisp is written in Oaklisp, so compiling the
# sources to bytecode and linking the cold world need a running
# Oaklisp, or a stand-in for one.  There are three ways, each with its
# own target, and make picks one when it runs, from what is there;
# configure has no say in it:
#
#   build-from-old-world  compile and cold-link (tool.oak) with an
#                         Oaklisp: the emulator just built, or
#                         $(OAKLISP), running $(OLD_WORLD)
#   build-from-bytecode   copy the prebuilt objects from
#                         $(PREBUILT_BYTECODE_DIR); cold-link with
#                         oak-cold-linker
#   build-from-guile      compile with the Guile-hosted compiler
#                         (src/cold-compiler, needs Guile 3); cold-link
#                         with oak-cold-linker
#
# Plain "make" builds oakworld.bin the first of these ways that is
# possible: from an old world if $(OLD_WORLD) exists (one saved by
# rebuild-world, or prebuilt/'s world for this machine), else from
# prebuilt bytecode, else with Guile.  Whichever way, the result is
# the same (see the check-* targets), the newly built emulator boots
# the cold world and loads the remaining layers, and rebuild-world
# then makes the system self-hosting.  The methods can also be set by
# hand: make OAK_COMPILE=world|prebuilt|guile OAK_COLD_LINK=world|c|guile.
#
# The pieces:
#   $(OLD_WORLD)   the world compiled and linked with, and where
#                  rebuild-world saves the world just built.  Default:
#                  old-oakworld.bin here if it exists, else prebuilt/'s
#                  world for this machine if that exists, else
#                  old-oakworld.bin.  Any world of the emulator's
#                  architecture will do, e.g. an installed one.
#   $(OAKLISP)     the emulator that runs $(OLD_WORLD): the one just
#                  built.  Set it (with a matching OLD_WORLD) to build
#                  for another word size with an installed Oaklisp:
#                  make OAKLISP=/usr/bin/oaklisp OLD_WORLD=/usr/lib/oaklisp/oakworld.bin
#   $(OAKBOOT)     the emulator just built, which boots the new world.

OAK_HOST_ARCH = @OAK_HOST_ARCH@
OAK_BYTECODE_ARCH = @OAK_BYTECODE_ARCH@

PREBUILT_DIR = $(top_srcdir)/prebuilt
PREBUILT_WORLD = $(PREBUILT_DIR)/src/world/$(OAK_HOST_ARCH)/oakworld.bin
PREBUILT_BYTECODE_DIR = $(PREBUILT_DIR)/src/world/bc2-32

OLD_WORLD = $(firstword $(wildcard old-oakworld.bin $(PREBUILT_WORLD)) old-oakworld.bin)

OAKLISP = $(abs_top_builddir)/src/emulator/oaklisp
OAK = $(OAKLISP) --batch
OAKBOOT = $(abs_top_builddir)/src/emulator/oaklisp --batch

# Flags for the emulator.
# OAKFLAGS = --trace-gc=1
OAKFLAGS =

# Which way, unless told otherwise.
WORLD_METHOD = $(if $(wildcard $(OLD_WORLD)),oldworld,$(if $(wildcard $(PREBUILT_BYTECODE_DIR)/kernel0.oa),bytecode,$(if $(filter-out no,$(GUILE)),guile,none)))

OAK_COMPILE = $(OAK_COMPILE_$(WORLD_METHOD))
OAK_COMPILE_oldworld = world
OAK_COMPILE_bytecode = prebuilt
OAK_COMPILE_guile = guile
OAK_COMPILE_none = none

OAK_COLD_LINK = $(OAK_COLD_LINK_$(WORLD_METHOD))
OAK_COLD_LINK_oldworld = world
OAK_COLD_LINK_bytecode = c
OAK_COLD_LINK_guile = c
OAK_COLD_LINK_none = c

build-from-old-world:
	@test -f "$(OLD_WORLD)" || { echo "build-from-old-world: no $(OLD_WORLD)"; exit 1; }
	$(MAKE) $(AM_MAKEFLAGS) OAK_COMPILE=world OAK_COLD_LINK=world $(ALLOAFILES) oakworld.bin

build-from-bytecode:
	@test -f "$(PREBUILT_BYTECODE_DIR)/kernel0.oa" || { echo "build-from-bytecode: no bytecode in $(PREBUILT_BYTECODE_DIR)"; exit 1; }
	$(MAKE) $(AM_MAKEFLAGS) OAK_COMPILE=prebuilt OAK_COLD_LINK=c $(ALLOAFILES) oakworld.bin

build-from-guile:
	@test x$(GUILE) != xno || { echo "build-from-guile: no Guile 3 (configure --with-guile)"; exit 1; }
	$(MAKE) $(AM_MAKEFLAGS) OAK_COMPILE=guile OAK_COLD_LINK=c $(ALLOAFILES) oakworld.bin

.PHONY: build-from-old-world build-from-bytecode build-from-guile

# The Guile-hosted Oaklisp (src/cold-compiler): it loads the world's
# own macros, compiler and linker from their .oak sources and runs
# them, so its objects are byte for byte what a native Oaklisp would
# produce.  Building its world takes some seconds, so it compiles
# everything in one run, recorded by guile.stamp (see below).
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

# An object's extra prerequisites and recipe, by $(OAK_COMPILE).

OA_PREREQ = $(OA_PREREQ_$(OAK_COMPILE))
OA_COMPILE = $(OA_COMPILE_$(OAK_COMPILE))

# With a world.  The compiler reads NAME.oak and writes NAME.oa in the
# current directory, so in a VPATH build the source is copied here
# first (keeping its time, so that guile.stamp stays newer).
#
# The bootstrap world may be older than the sources: its assembler may
# not know an instruction or register the sources use, and a call to
# an open coded operation it does not know would be compiled as an
# ordinary call, which such an operation cannot answer.  So the
# assembler is compiled first and loaded into the bootstrap world's
# compiler before anything else is compiled, and the few files that
# use new open coded operations from other files get those loaded
# too (OAKEXTRALOAD, below).  (A current bootstrap world compiles
# everything the same either way.)
OAKPRELOAD = --locale compiler-locale --load assembler --locale system-locale
OAKEXTRALOAD =

OA_PREREQ_world = assembler.oa
OA_COMPILE_world = cmp -s $< $(@:.oa=.oak) || cp -p $< $(@:.oa=.oak); \
	$(OAK) $(OAKFLAGS) --world $(OLD_WORLD) -- $(OAKPRELOAD) $(OAKEXTRALOAD) \
	   $(OAKLOCALE) --target $(OAK_BYTECODE_ARCH) --compile $(@:.oa=) --exit

# From prebuilt bytecode: no running Oaklisp.  That includes
# system-version.oa, so the world reports the version the bytecode
# was compiled from until it is rebuilt with itself; a warning says
# so when that differs.
OA_PREREQ_prebuilt =
OA_COMPILE_prebuilt = cp $(PREBUILT_BYTECODE_DIR)/$@ $@$(if $(filter system-version.oa,$@), \
	&& { grep -q '"$(PACKAGE_STRING)"' $@ || echo "warning: the prebuilt bytecode was compiled for another version of Oaklisp; the world will report that version until it is rebuilt with itself (make rebuild-world)"; })

# From source only, with Guile: guile.stamp compiles everything in one
# run (below); an object that is missing anyway makes it run again.
# The stamp matters only to this method.
OA_PREREQ_guile = guile.stamp
OA_COMPILE_guile = @test -f $@ || { rm -f guile.stamp; $(MAKE) $(AM_MAKEFLAGS) guile.stamp; }

# No way at all.
OA_PREREQ_none =
OA_COMPILE_none = @echo "no way to build the world: no old world ($(OLD_WORLD)), no prebuilt bytecode in $(PREBUILT_BYTECODE_DIR), and no Guile 3 (configure --with-guile); see BUILD.md"; exit 1

%.oa: %.oak $(OA_PREREQ)
	$(OA_COMPILE)

# assembler.oak is compiled without preloading itself (and would
# otherwise depend on itself); multiproc.oak uses the %THREADS-ENABLED?
# operation multi-em.oak defines, and file-io.oak's RENAME-FILE uses
# (%STREAM-PRIMITIVE 14) from streams.oak.
assembler.oa: OAKPRELOAD =
assembler.oa: assembler.oak $(filter-out assembler.oa, $(OA_PREREQ))
	$(OA_COMPILE)

multiproc.oa: OAKEXTRALOAD = --load multi-em
multiproc.oa: multi-em.oa

file-io.oa: OAKEXTRALOAD = --load streams
file-io.oa: streams.oa

# system-version.oak is generated by configure into the build
# directory, hence the ./ in GUILESRCS, which keeps the compiler from
# looking for it in srcdir.
guile.stamp: $(ALLOAFILESNONGEN:.oa=.oak) system-version.oak $(GUILEHOST)
	$(OAKGUILE) --outdir . $(GUILESRCS)
	touch $@

##### Linking

# The cold world's extra prerequisites and linker, by $(OAK_COLD_LINK).

LINK_PREREQ_world = $(TOOLFILES)
LINK_COLD_world = $(OAK) $(OAKFLAGS) --world $(OLD_WORLD) \
	   -- \
	   --load tool \
	   --eval "(tool-files '($(COLDFILESD:.oa=)) 'new \"$(OAK_BYTECODE_ARCH)\")" \
	   --exit

LINK_PREREQ_c =
LINK_COLD_c = ../emulator/oak-cold-linker --target $(OAK_BYTECODE_ARCH) -o new \
	   $(COLDFILESD:.oa=)

LINK_PREREQ_guile = $(GUILEHOST)
LINK_COLD_guile = $(OAKGUILE) --load tool \
	   --eval "(tool-files '($(COLDFILESD:.oa=)) \"new\" \"$(OAK_BYTECODE_ARCH)\")"

new.cold: $(COLDFILES) $(LINK_PREREQ_$(OAK_COLD_LINK))
	-rm -f new.cold new.sym
	$(LINK_COLD_$(OAK_COLD_LINK))

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

##### Rebuilding the world with itself

# "make rebuild-world" saves the world just built as $(OLD_WORLD),
# removes everything the world build made (clean-world), and makes it
# all again from scratch with the saved world (build-from-old-world).
# If there is no world to save but $(OLD_WORLD) exists, it rebuilds
# from that.  One rebuild on top of any build gives a self-hosted
# world; "make bootstrap" at the top level does two and then checks
# the fixpoint (check-fixpoint, below).  Makefile-vars, if
# regenerated, is made by $(OLD_WORLD) too.
#
# clean-world leaves guile.stamp alone: it belongs to the Guile
# method, which the rebuild does not use.

rebuild-world:
	@if test -f oakworld.bin; then \
	  $(MKDIR_P) `dirname $(OLD_WORLD)` && cp oakworld.bin $(OLD_WORLD) \
	    && echo "rebuild-world: saved oakworld.bin as $(OLD_WORLD)"; \
	elif test -f "$(OLD_WORLD)"; then \
	  echo "rebuild-world: no oakworld.bin; rebuilding from $(OLD_WORLD)"; \
	else \
	  echo "rebuild-world: no oakworld.bin and no $(OLD_WORLD) to rebuild from"; exit 1; \
	fi
	$(MAKE) $(AM_MAKEFLAGS) clean-world
	$(MAKE) $(AM_MAKEFLAGS) build-from-old-world

clean-world:
	rm -f $(ALLOAFILES) new.cold new.sym oakworld-?.bin oakworld.bin

.PHONY: rebuild-world clean-world

##### Compiling everything with a finished world

# Compile the sources $$files (base names, in the current directory)
# with the emulator just built running the world $$world, for --target
# $$target, each in its own locale, logging each to NAME.log.  This is
# how the fixpoint check and the prebuilt refresh compile; a finished
# world needs none of the preloading above.
COMPILE_ALL = for f in $$files; do \
	  loc=""; \
	  case " $(COMPFILES:.oa=) " in *" $$f "*) loc="--locale compiler-locale";; esac; \
	  case " $(RNRSFILES:.oa=) " in *" $$f "*) loc="--locale scheme-locale";; esac; \
	  $(OAKBOOT) $(OAKFLAGS) --world $$world \
	    -- $$loc --target $$target --compile $$f --exit > $$f.log 2>&1 \
	    || { echo "compile failed: $$f (see `pwd`/$$f.log)"; exit 1; }; \
	done

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
	cd prebuilt-tmp && files='$(ALLOAFILES:.oa=)' \
	  world=$(abs_builddir)/oakworld.bin target=bc2-32 && $(COMPILE_ALL)
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
	$(OAK) $(OAKFLAGS) --world $(OLD_WORLD) \
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
	@cd fixpoint-check && files='$(ALLOAFILES:.oa=)' \
	  world=$(abs_builddir)/oakworld.bin target=$(OAK_BYTECODE_ARCH) && $(COMPILE_ALL)
	@rc=0; cd fixpoint-check && for f in $(ALLOAFILES); do \
	  cmpbody() { tail -n +2 "$$1" > cmp1.tmp; tail -n +2 "$$2" > cmp2.tmp; cmp -s cmp1.tmp cmp2.tmp; }; \
	  cmpbody $$f ../$$f \
	    || { echo "$$f: recompiled object differs from the one built from"; rc=1; }; \
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

CLEANFILES = guile.stamp

clean-local: clean-world
	rm -rf prebuilt-tmp fixpoint-check guile-check

# Remove the .oak sources copied into a VPATH build directory, and
# the world saved by rebuild-world (kept by "make clean" so that
# build-from-old-world can be repeated).
distclean-local:
	if test "$(srcdir)" != .; then rm -f $(ALLOAFILESNONGEN:.oa=.oak); fi
	rm -f old-oakworld.bin

EXTRA_DIST = $(ALLOAFILESNONGEN:.oa=.oak) $(GRAVY)
