# This value of prefix will usually be overridden
prefix=/usr/local

.PHONY: all clean

all clean:
	$(MAKE) -C lang $@
	$(MAKE) -C lim  $@

.PHONY: install
install:
	$(MAKE) -C lang lang.ps
	$(MAKE) -C lim  lim.ps
	cp -a lang/lang.ps lim/lim.ps $(DESTDIR)$(prefix)/share/doc/oaklisp/
