.PHONY: all install clean
all install clean:
	$(MAKE) -C src $@
	$(MAKE) -C doc $@
	$(MAKE) -C man $@

RELNAME := $(shell date +'%d-%b-%Y')

RTARGET=oaklisp-$(RELNAME)

.PHONY: release

release: Makefile README
	mkdir $(RTARGET)
	cp -a $^ $(RTARGET)/
	mkdir $(RTARGET)/src
	$(MAKE) -C src RTARGET=../$(RTARGET)/src release
	mkdir $(RTARGET)/doc
	$(MAKE) -C doc RTARGET=../$(RTARGET)/doc release
	mkdir $(RTARGET)/man
	cp -a man/Makefile $(RTARGET)/man/
	mkdir $(RTARGET)/man/man1
	cp -a man/man1/oaklisp.1 $(RTARGET)/man/man1/
	mkdir $(RTARGET)/debian
	cp -a debian/{rules,README.debian,changelog,control,copyright,dirs,menu} $(RTARGET)/debian/
	tar zcf $(RTARGET).tar.gz $(RTARGET)
