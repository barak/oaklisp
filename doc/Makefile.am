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

LATEX_SRCS = lang/lang.tex lim/lim.tex summary/OaklispSummary.tex
LATEX_PDFS = lang/lang.pdf lim/lim.pdf summary/OaklispSummary.pdf

pdf_DATA = $(LATEX_PDFS)

CLEANFILES = $(LATEX_PDFS)

EXTRA_DIST = $(LATEX_SRCS)

RUBBERFLAGS = --inplace --warn=all --pdf

.tex.pdf:
	$(RUBBER) $(RUBBERFLAGS) $< \
	 || cp ../prebuilt/doc/$@ $@

clean-local:
	-$(RUBBER) --clean $(RUBBERFLAGS) $(PAPER_SRCS)

EXTRA_DIST += examples/bank-example.oak examples/change.oak		\
 examples/test-bank-example.oak examples/unit-testing.oak		\
 lang/control.tex lang/cover.tex lang/dynamic.tex lang/intro.tex	\
 lang/io.tex lang/locales.tex lang/methods.tex lang/misc.tex		\
 lang/numbers.tex lang/numhier.eps lang/seqhier.eps			\
 lang/sequences.tex lang/sides.tex lang/types.tex lang/user.tex		\
 lim/admin.tex lim/boot.tex lim/bytecode.tex lim/compiler.tex		\
 lim/cover.tex lim/dataform.tex lim/intro.tex lim/language.tex		\
 lim/methods.tex lim/oaklevel.tex lim/stack.tex mandefs.tex		\
 oakman.bib
