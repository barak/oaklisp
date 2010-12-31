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

all: lang.pdf

SRC = control.tex cover.tex dynamic.tex intro.tex io.tex lang.tex \
      locales.tex methods.tex misc.tex numbers.tex sequences.tex \
      sides.tex types.tex user.tex

OSRC = seqhier.ips numhier.ips

lang.dvi: $(SRC) $(OSRC) ../mandefs.tex
	-rm lang.ind
	-latex -interaction=nonstopmode lang
	bibtex lang
	makeindex lang
	latex lang
	latex lang

lang.ps: lang.dvi $(OSRC)
	dvips -K -tletter -Ppdf -G0 -R0 -q -o $@ lang

%.pdf: %.ps
	ps2pdf $<

#lang.ind: lang.idx
#	makeindex lang

.PHONY: clean
clean:
	-rm -f lang.bbl lang.blg lang.dvi lang.ps lang.pdf
	-rm -f lang.idx lang.ilg lang.ind lang.log lang.toc
	-rm -f *.aux
