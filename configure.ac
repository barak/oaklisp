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
AC_INIT([Oaklisp],[1.3.6],[bap+oaklisp@debian.org],[oaklisp],[http://oaklisp.alioth.debian.org/])
AC_CONFIG_SRCDIR([src/emulator/oaklisp.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall])

# Process options
AC_MSG_CHECKING([enable_docs])
AC_ARG_ENABLE([docs],
       [AS_HELP_STRING([--enable-docs],[build and install LaTeX-format documentation (default=yes)])],,
       [enable_docs=yes])
AC_MSG_RESULT([$enable_docs])
AM_CONDITIONAL([DOCS], [test x${enable_docs} = xyes])

AC_MSG_CHECKING([enable_ndebug])
AC_ARG_ENABLE([ndebug],
       [AS_HELP_STRING([--enable-ndebug],[enable high speed disable debugging (default=yes)])],,
       [enable_ndebug=yes])
AC_MSG_RESULT([$enable_ndebug])
AM_CONDITIONAL([NDEBUG], [test x${enable_ndebug} = xyes])

AC_MSG_CHECKING([enable_threads])
AC_ARG_ENABLE([threads],
       [AS_HELP_STRING([--enable-threads],[enable thread support (default=no)])],,
       [enable_threads=no])
AC_MSG_RESULT([$enable_threads])
AM_CONDITIONAL([ENABLE_THREADS], [test x${enable_threads} = xyes])

AC_MSG_CHECKING([with_world])
AC_ARG_WITH([world],
       [AS_HELP_STRING([--with-world[=WORLD]],[for bootstrapping (default: searches)])],,
       [with_world=check])
AC_MSG_RESULT([$with_world])

AC_MSG_CHECKING([with_oaklisp])
AC_ARG_WITH([oaklisp],
       [AS_HELP_STRING([--with-oaklisp=OAKLISP],[for bootstrapping (default=no)])],,
       [with_oaklisp=no])
AC_MSG_RESULT([$with_oaklisp])

AS_IF([test x${with_oaklisp} = xcheck || test x${with_oaklisp} = xyes],
      [AC_PATH_PROG([with_oaklisp],
                    [oaklisp],
                    [AS_IF([test x${with_oaklisp} = xyes],
                           [AC_MSG_ERROR([no oaklisp executable found])],
                           [no])])])

# Check if we are on a non-32bit-pointer memory model.
# If so, try to enable compiler options for a 32-bit memory model.
# For example, -m32 on x86_64 aka amd64.
# (This is easier than doing a 64-bit port!)
#
# This should go before any checks that actually invoke the compiler,
# so those tests will be in 32-bit mode.  CFLAGS is passed to the
# linker, so the flag does not need to be set there, but it is
# included in a comment in case this ever changes.
#
# The options here are for GCC, but -m32 and -mx32 works on amd64 with
# clang, and others might as well.

AC_CHECK_SIZEOF([void *])
AS_IF([test x"$ac_cv_sizeof_void_p" = x4],
 [AC_MSG_NOTICE([pointer size matches expectations])],
 [AC_MSG_WARN([non 32-bit pointer platform detected])
  AC_MSG_NOTICE([trying to enable 32-bit memory model compiler option for ${host_cpu}])
  AC_CANONICAL_HOST
  AS_CASE([${host_cpu}],
   [alpha*],              [AC_MSG_ERROR([32-bit memory model unavailable on ${host_cpu}])],
   [amd64* | x86_64],     [flag32="-m32"],
   [arm64*],              [flag32="-mabi=ilp32"],
   [ia64*],               [flag32="-milp32"],
   [mips64*],             [flag32="-mabi=32"],
   [powerpc64* | ppc64*], [flag32="-m32"],
   [s390x*],              [flag32="-m31"],
   [sparc64*],            [flag32="-m32"],
                          [flag32="-m32"
                           AC_MSG_WARN([non-32-bit host CPU ${host_cpu}])
			   AC_MSG_WARN([no 32-bit memory model option known, trying -m32])])
  AC_MSG_NOTICE([compiler option for 32-bit memory model: ${flag32}])
  CFLAGS="$CFLAGS ${flag32}"
  : LDFLAGS="$LDFLAGS ${flag32}"
  unset ac_cv_sizeof_void_p
  AC_CHECK_SIZEOF([void *])
  AS_IF([test x"$ac_cv_sizeof_void_p" = x0],
   [AC_MSG_ERROR([bad flags, Indy])])
  AS_IF([test x"$ac_cv_sizeof_void_p" != x4],
   [AC_MSG_WARN([bad news, pointers are now $ac_cv_sizeof_void_p bytes])])])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_SED
AC_CHECK_PROGS([INDENT],[indent],[true])
AS_IF([test x${enable_docs} = xyes],
 [AC_CHECK_PROGS([RUBBER],[rubber],[false])])

# Checks for libraries.
AS_IF([test x$threads = xyes],
 [AC_SEARCH_LIBS([pthread_create],[pthread])])

# Checks for header files.
AC_CHECK_HEADERS([stddef.h stdlib.h string.h sys/time.h unistd.h],,
 [AC_MSG_ERROR([required header file unavailable])])

# Checks for typedefs, structures, and compiler characteristics.
AX_CFLAGS_WARN_ALL
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AX_C_LONG_LONG
AC_TYPE_SIZE_T
AC_C_BIGENDIAN([worldsubdir=eb32],
               [worldsubdir=el32],
               [AC_MSG_WARN([missing case, endianity unknown])],
               [AC_MSG_WARN([missing case, endianity unversal])])

# Actual scan for binary world find comes here rather than in option
# processing because we need to know endianity.

# snippet of code for other ways to find default oakworld.bin
#  oaklisp --help | egrep -A1 '[-][-]world' | tail -n +2

AC_MSG_NOTICE([worldsubdir=${worldsubdir}])

AS_IF([test x${with_world} = xcheck || test x${with_world} = xyes],
 [AC_MSG_CHECKING([possible locations for oakworld.bin])
  f=$(ls $(pwd)/src/world/oakworld.bin \
         $(pwd)/prebuilt/src/world/${worldsubdir}/oakworld.bin \
         $(pwd)/prebuilt/src/world/oakworld.bin \
         ${HOME}/lib/oaklisp/oakworld.bin \
         /usr/local/lib/oaklisp/oakworld.bin \
         /usr/lib/oaklisp/oakworld.bin \
         /usr/lib/*/oaklisp/oakworld.bin \
         /lib/oaklisp/oakworld.bin \
         /lib/*/oaklisp/oakworld.bin 2> /dev/null | head -1)
  AC_MSG_RESULT([found ${f}])
  AC_CHECK_FILE([${f}],
                [with_world=${f}])])

AS_IF([test x${with_world} = xyes],
 [AC_MSG_ERROR([no oakworld.bin found])])

AS_IF([test x${with_world} != xno && test x${with_world} != xcheck],
 [AC_SUBST([OAKWORLDFLAGS],[--world=${with_world}])])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([strerror],,
 [AC_MSG_ERROR([required library function unavailable])])
AC_CHECK_FUNCS([GetTickCount getrusage gettimeofday clock])

# Epilogue
AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 man/Makefile
                 src/Makefile
                 src/emulator/Makefile
                 src/world/Makefile
                 src/world/system-version.oak])
AC_OUTPUT
