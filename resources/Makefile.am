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


# Desktop entry and its icon, in the freedesktop.org places (the icon
# is scalable, so it goes under hicolor/scalable and needs no sizes),
# and command line completion for bash, zsh and fish.

desktopdir = $(datadir)/applications
dist_desktop_DATA = oaklisp.desktop

icondir = $(datadir)/icons/hicolor/scalable/apps
dist_icon_DATA = oaklisp.svg

# bash wants the file named after the command, which would make a
# confusing name here and, with a copying rule, debris in the source
# directory of an in-tree build; so install it by hand instead.
bashcompletiondir = $(datadir)/bash-completion/completions
EXTRA_DIST = oaklisp.bash

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(bashcompletiondir)
	$(INSTALL_DATA) $(srcdir)/oaklisp.bash \
	  $(DESTDIR)$(bashcompletiondir)/oaklisp

uninstall-local:
	rm -f $(DESTDIR)$(bashcompletiondir)/oaklisp

zshcompletiondir = $(datadir)/zsh/site-functions
dist_zshcompletion_DATA = _oaklisp

fishcompletiondir = $(datadir)/fish/vendor_completions.d
dist_fishcompletion_DATA = oaklisp.fish
