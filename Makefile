.PHONY: all install clean
all install clean:
	$(MAKE) -C src $@
	$(MAKE) -C doc $@
	$(MAKE) -C man $@
