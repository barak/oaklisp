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

# This value of prefix will usually be overridden
prefix=/usr/local

.PHONY: all clean

all clean:
	$(MAKE) -C lang $@
	$(MAKE) -C lim  $@

d=$(DESTDIR)$(prefix)/share/doc/oaklisp

.PHONY: install
install:
	$(MAKE) -C lang lang.pdf
	$(MAKE) -C lim  lim.pdf
	mkdir --parents $d
	cp -a lang/lang.pdf lim/lim.pdf $d/
