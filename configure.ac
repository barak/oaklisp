#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

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

# Prologue
AC_PREREQ([2.69])
AC_INIT([Oaklisp],[1.3.4],[bap+oaklisp@debian.org],[oaklisp],[http://oaklisp.alioth.debian.org/])
AC_CONFIG_SRCDIR([src/emulator/oaklisp.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall])

# Checks for options
dnl --enable-threads
dnl --enable-ndebug (CPPFLAGS += -DFAST)
dnl --with-world (for the default world in bootstrapping)
dnl --withoaklisp (for a working oaklisp in bootstrapping)
dnl snippet of code to find default oakworld.bin
dnl    oaklisp -h | egrep -A1 '[-][-]world' | tail -n +2

# No double there is a "right" way of doing this.
AM_CONDITIONAL(USE_M32, [test "$(uname --machine)" = "x86_64"])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_CHECK_PROGS([INDENT],[indent],[true])
AC_CHECK_PROGS([RUBBER],[rubber],[false])

# Checks for libraries.
AC_SEARCH_LIBS([pthread_create],[pthread])

# Checks for header files.
AC_CHECK_HEADERS([stddef.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AX_CFLAGS_WARN_ALL
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday strerror])

# Epilogue
AC_CONFIG_FILES([Makefile doc/Makefile src/Makefile src/world/Makefile src/world/top-level.oak])
AC_OUTPUT
