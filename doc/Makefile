.PHONY: all release

all:
	$(MAKE) -C lang all
	$(MAKE) -C lim  all

release: Makefile mandefs.tex oakman.bib
	cp -a $^ $(RTARGET)/
	mkdir $(RTARGET)/lang
	$(MAKE) -C lang RTARGET=../$(RTARGET)/lang release
	mkdir $(RTARGET)/lim
	$(MAKE) -C lim  RTARGET=../$(RTARGET)/lim  release
