# Bigendian machines need bigendian binary oaklisp worlds, so non-cold
# worlds are only compatible across machines of the same endianity.

all: oakworld.bin

-include Makefile-vars

ALLOAFILES = $(TOOLFILES) $(MAKEFILES) $(FILESFILES) $(COLDFILES) \
		$(MISCFILES) $(COMPFILES) $(RNRSFILES)

SOURCE = $(ALLOAFILES:.oa=.oak) Makefile

# The Oaklisp emulator executable
OAK=oakem

# Flags for the emulator
OAKFLAGS=-trace-gc=1

# How to compile oaklisp source files into oaklisp bytecode object files:
.SUFFIXES: .oa .oak

%.oa : %.oak ; $(OAK) $(OAKFLAGS) -- --compile $* --exit

# How to build a new cold world using the world builder tool:
new.cold: $(COLDFILES) $(TOOLFILES)
	-rm -f new.cold new.sym
	$(OAK) $(OAKFLAGS) \
	   -- \
	   --load tool \
	   --eval "(tool-files '($(COLDFILESD:.oa=)) 'new)" \
	   --exit

# How to boot cold world into warm world:

oakworld-1.bin: new.cold
	 $(OAK) $(OAKFLAGS) --world new.cold --dump $@

# load successive layers of functionality onto the world

oakworld-2.bin: oakworld-1.bin $(MISCFILES)
	$(OAK) $(OAKFLAGS) --world oakworld-1.bin --dump $@ \
	   -- \
	   $(foreach f, $(MISCFILES:.oa=), --load $(f)) \
	   --exit

oakworld-3.bin: oakworld-2.bin $(COMPFILES)
	 $(OAK) $(OAKFLAGS) --world oakworld-2.bin --dump $@ \
	   -- \
	   --locale compiler-locale \
	   $(foreach f, $(COMPFILES:.oa=), --load $(f)) \
	   --locale system-locale --exit

oakworld.bin: oakworld-3.bin $(RNRSFILES)
	$(OAK) $(OAKFLAGS) --world oakworld-3.bin --dump $@ \
	-- \
	-eval '(define-instance scheme-locale locale (list system-locale))'\
	-locale scheme-locale \
	$(foreach f, $(RNRSFILES:.oa=), --load $(f)) \
	-locale system-locale --exit

# How to rebuild variables included by this Makefile itself:

Makefile-vars: $(MAKEFILES) $(FILESFILES)
	$(OAK) $(OAKFLAGS) \
	   -- \
	   --load files --load make-makefile --eval '(make-makefile "$@")' \
	   --exit

.PHONY: install
install: oakworld.bin
	cp -a $^ ../lib/

.PHONY: clean
clean: clean-oa-files
	-rm -f oakworld{-1,-2,-3,}.bin new.cold new.sym

.PHONY: clean-oa-files
clean-oa-files:
	-rm -f $(ALLOAFILES)

## What goes in an official release:

.PHONY: release
release: $(SOURCE) $(GRAVY) Makefile-vars
	cp -a $^ $(RTARGET)/
