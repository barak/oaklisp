# This value of prefix will usually be overridden
prefix=/usr/local

.PHONY: all install clean

all clean:
	$(MAKE) -C emulator $@
	$(MAKE) -C world $@

install:
	$(MAKE) -C emulator CPPFLAGS=-DFAST install
	$(MAKE) -C world    install
