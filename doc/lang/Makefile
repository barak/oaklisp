all: lang.dvi

SRC = control.tex cover.tex dynamic.tex intro.tex io.tex lang.tex \
      locales.tex methods.tex misc.tex numbers.tex sequences.tex \
      sides.tex types.tex user.tex

OSRC = seqhier.ips numhier.ips

lang.dvi: $(SRC) $(OSRC) ../mandefs.tex
	-rm lang.ind
	-latex lang </dev/null
	bibtex lang
	makeindex lang
	latex lang

lang.ps: lang.dvi $(OSRC)
	dvips -o lang.ps lang

#lang.ind: lang.idx
#	makeindex lang

.PHONY: release
release: Makefile $(SRC) $(OSRC)
	cp -a $^ $(RTARGET)/
