.PHONY: all install clean

all:
	$(MAKE) -C emulator all
	$(MAKE) -C world all

install:
	$(MAKE) -C emulator install
	$(MAKE) -C world install

clean:
	$(MAKE) -C emulator clean
	$(MAKE) -C world clean


.PHONY: release
release: GPL Makefile CREDITS
	cp -a $^ $(RTARGET)/
	mkdir $(RTARGET)/bin
	cp -a bin/oaklisp $(RTARGET)/bin/
	mkdir $(RTARGET)/lib
	cp -a lib/oakworld.bin $(RTARGET)/lib/
	mkdir $(RTARGET)/man
	mkdir $(RTARGET)/man/man1
	cp -a man/man1/oaklisp.1 $(RTARGET)/man/man1/
	mkdir $(RTARGET)/emulator
	$(MAKE) -C emulator RTARGET=../$(RTARGET)/emulator release
	mkdir $(RTARGET)/world
	$(MAKE) -C world    RTARGET=../$(RTARGET)/world    release
