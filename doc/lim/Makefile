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

all: lim.dvi

SRC = admin.tex boot.tex bytecode.tex compiler.tex cover.tex \
      dataform.tex intro.tex language.tex lim.tex methods.tex \
      oaklevel.tex stack.tex

lim.dvi: $(SRC) ../mandefs.tex
	-rm lim.ind
	-latex lim </dev/null
	bibtex lim
	makeindex lim
	latex lim

lim.ps: lim.dvi
	dvips -o lim.ps lim

.PHONY: clean
clean:
	-rm -f lim.{bbl,blg,dvi,ps,idx,ilg,ind,log,toc} *.aux
