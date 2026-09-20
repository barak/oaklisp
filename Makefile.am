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

SUBDIRS = src man tests

if DOCS
SUBDIRS += doc
endif

dist_doc_DATA = README.md AUTHORS COPYING BUILD.md ChangeLog NEWS

# Desktop entry and its icon, in the freedesktop.org places (the icon
# is scalable, so it goes under hicolor/scalable and needs no sizes).
desktopdir = $(datadir)/applications
dist_desktop_DATA = resources/oaklisp.desktop

icondir = $(datadir)/icons/hicolor/scalable/apps
dist_icon_DATA = resources/oaklisp.svg

# Refresh the prebuilt bootstrap material in prebuilt/ from what was
# just built: bytecode, a world image for this architecture, the
# emulator's instruction table, and the documentation.
prebuilt:
	$(MAKE) -C src/world prebuilt
	$(MAKE) -C src/emulator prebuilt
if DOCS
	$(MAKE) -C doc prebuilt
endif

.PHONY: prebuilt

# "make check" runs the test suite in tests/; "make bench" the benchmarks.
bench:
	$(MAKE) -C tests bench

.PHONY: bench

# Self-hosting.  "make rebuild-world" saves the world just built
# (however it was built: from an old or prebuilt world, from prebuilt
# bytecode, or from source alone with Guile) and rebuilds all the
# objects and the world from scratch with it; it can be repeated, and
# rebuilds from the saved world when there is no world to save.  "make
# bootstrap" builds everything, rebuilds the world with itself twice,
# and checks that the result reproduces its own objects, so that a
# following "make check" and "make install" use a self-hosted system.
# See src/world/Makefile.am.
bootstrap: all
	$(MAKE) $(AM_MAKEFLAGS) -C src/world rebuild-world
	$(MAKE) $(AM_MAKEFLAGS) -C src/world rebuild-world
	$(MAKE) $(AM_MAKEFLAGS) -C src/world check-fixpoint

rebuild-world:
	$(MAKE) $(AM_MAKEFLAGS) -C src/emulator all
	$(MAKE) $(AM_MAKEFLAGS) -C src/world rebuild-world

check-fixpoint:
	$(MAKE) $(AM_MAKEFLAGS) -C src/world check-fixpoint

.PHONY: bootstrap rebuild-world

# Remove all generated files, restoring the tree to pre-autoreconf state.
# (Run "autoreconf --install && ./configure" to rebuild.)
realclean: maintainer-clean
	rm -rf autom4te.cache
	rm -f aclocal.m4 compile config.guess config.h.in config.sub \
	      configure depcomp install-sh missing INSTALL stamp-h1
	find . -name Makefile.in -delete
	find . -name .deps -type d -exec rm -rf {} +
