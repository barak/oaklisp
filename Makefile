.PHONY: all

all:
	$(MAKE) -C src all
	$(MAKE) -C doc all

RELNAME := $(shell date +'%d-%b-%Y')

RTARGET=oaklisp-$(RELNAME)

.PHONY: release

release: Makefile
	mkdir $(RTARGET)
	cp -a $^ $(RTARGET)/
	mkdir $(RTARGET)/src
	$(MAKE) -C src RTARGET=../$(RTARGET)/src release
	mkdir $(RTARGET)/doc
	$(MAKE) -C doc RTARGET=../$(RTARGET)/doc release
	tar cf $(RTARGET).tar $(RTARGET)
