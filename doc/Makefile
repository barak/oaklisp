# This value of prefix will usually be overridden
prefix=/usr/local

.PHONY: all release clean

all clean:
	$(MAKE) -C lang $@
	$(MAKE) -C lim  $@

release: Makefile mandefs.tex oakman.bib
	cp -a $^ $(RTARGET)/
	mkdir $(RTARGET)/lang
	$(MAKE) -C lang RTARGET=../$(RTARGET)/lang release
	mkdir $(RTARGET)/lim
	$(MAKE) -C lim  RTARGET=../$(RTARGET)/lim  release

.PHONY: install
install:
	$(MAKE) -C lang lang.ps
	$(MAKE) -C lim  lim.ps
	cp -a lang/lang.ps lim/lim.ps $(DESTDIR)$(prefix)/share/doc/oaklisp/
