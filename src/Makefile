# This value of prefix will usually be overridden
prefix=/usr/local

.PHONY: all install clean

all clean:
	$(MAKE) -C emulator $@
	$(MAKE) -C world $@

install:
	$(MAKE) -C emulator CPPFLAGS=-DFAST install
	$(MAKE) -C world    install

.PHONY: release
release: COPYING Makefile CREDITS
	cp -a $^ $(RTARGET)/
	mkdir $(RTARGET)/emulator
	$(MAKE) -C emulator RTARGET=../$(RTARGET)/emulator release
	mkdir $(RTARGET)/world
	$(MAKE) -C world    RTARGET=../$(RTARGET)/world    release
