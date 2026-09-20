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
$(LATEX_PDFS): .FORCE
.PHONY: .FORCE

pdf_DATA = $(LATEX_PDFS)

EXTRA_DIST = $(LATEX_SRCS)

# Each document also builds by hand in its own directory (cd lang;
# pdflatex lang; bibtex lang; ...), which the rules here preserve:
# latexmk runs in the source's directory (-cd), where the chapters,
# ../mandefs.tex and ../oakman.bib are, and writes everything to the
# corresponding directory here (-outdir), so an out-of-tree build
# leaves the source tree alone.  bibtex runs in the output directory,
# so ../oakman.bib has to be here too; and the EPS figures are
# converted here beforehand, rather than beside their sources by the
# epstopdf package (TeX looks in the output directory first).
.tex.pdf:
	$(MKDIR_P) `dirname $@`
	test -f oakman.bib || cp $(srcdir)/oakman.bib oakman.bib
	$(LATEXMK) -cd -pdf -outdir=$(abs_builddir)/`dirname $@` $< \
	 || cp $(top_srcdir)/prebuilt/doc/$@ $@

FIGURES = lang/numhier.pdf lang/seqhier.pdf
lang/lang.pdf: $(FIGURES)
.eps.pdf:
	$(MKDIR_P) `dirname $@`
	$(EPSTOPDF) --outfile=$@ $<

# Refresh the prebuilt copies.
prebuilt: $(LATEX_PDFS)
	for f in $(LATEX_PDFS); do \
	  $(MKDIR_P) $(top_srcdir)/prebuilt/doc/$$(dirname $$f); \
	  cp $$f $(top_srcdir)/prebuilt/doc/$$f; \
	done

.PHONY: prebuilt

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

CLEANFILES = $(LATEX_PDFS) $(FIGURES)

# Only an out-of-tree build has a copy of the bibliography here.
distclean-local:
	if test "$(srcdir)" != .; then rm -f oakman.bib; fi

CLEANFILES += lang/control.aux lang/cover.aux lang/dynamic.aux		\
 lang/intro.aux lang/io.aux lang/lang.aux lang/lang.bbl lang/lang.blg	\
 lang/lang.fdb_latexmk lang/lang.fls lang/lang.idx lang/lang.ilg	\
 lang/lang.ind lang/lang.log lang/lang.toc lang/locales.aux		\
 lang/methods.aux lang/misc.aux lang/numbers.aux			\
 lang/numhier-eps-converted-to.pdf lang/seqhier-eps-converted-to.pdf	\
 lang/sequences.aux lang/sides.aux lang/types.aux lang/user.aux

CLEANFILES += lim/admin.aux lim/boot.aux lim/bytecode.aux		\
 lim/compiler.aux lim/cover.aux lim/dataform.aux lim/intro.aux		\
 lim/language.aux lim/lim.aux lim/lim.bbl lim/lim.blg			\
 lim/lim.fdb_latexmk lim/lim.fls lim/lim.idx lim/lim.ilg lim/lim.ind	\
 lim/lim.log lim/lim.toc lim/methods.aux lim/oaklevel.aux		\
 lim/stack.aux

CLEANFILES += summary/OaklispSummary.aux			\
 summary/OaklispSummary.fdb_latexmk summary/OaklispSummary.fls	\
 summary/OaklispSummary.log
