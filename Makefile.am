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

SUBDIRS = src man

if DOCS
SUBDIRS += doc
endif

doc_DATA = README.md AUTHORS COPYING BUILD.md ChangeLog NEWS

# Remove all generated files, restoring the tree to pre-autoreconf state.
# (Run "autoreconf --install && ./configure" to rebuild.)
realclean: maintainer-clean
	rm -rf autom4te.cache
	rm -f aclocal.m4 compile config.guess config.h.in config.sub \
	      configure depcomp install-sh missing INSTALL stamp-h1
	find . -name Makefile.in -delete
	find . -name .deps -type d -exec rm -rf {} +
