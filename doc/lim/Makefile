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
