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

.PHONY: clean
clean:
	-rm -f lang.{bbl,blg,dvi,ps,idx,ilg,ind,log,toc} *.aux
