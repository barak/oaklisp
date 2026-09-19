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
AC_INIT([Oaklisp],[2.0.0~devel],[barak+oaklisp@pearlmutter.net],[oaklisp],[https://github.com/barak/oaklisp])
AC_CONFIG_SRCDIR([src/emulator/oaklisp.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects -Wall])

# Process options
AC_MSG_CHECKING([enable_64_bit])
AC_ARG_ENABLE([64-bit],
       [AS_HELP_STRING([--enable-64-bit],[use native 64-bit mode if available (default=yes)])],,
       [enable_64_bit=yes])
AC_MSG_RESULT([$enable_64_bit])

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
       [AS_HELP_STRING([--enable-threads],[enable native thread support with concurrent GC (default=no)])],,
       [enable_threads=no])
AC_MSG_RESULT([$enable_threads])
AM_CONDITIONAL([ENABLE_THREADS], [test x${enable_threads} = xyes])

# Bootstrapping: Oaklisp is written in Oaklisp, so building the world
# needs a running Oaklisp or a stand-in.  How that is done is decided
# by make from what is there (an old or prebuilt world, prebuilt
# bytecode, or Guile 3); see src/world/Makefile.am and BUILD.md.  All
# configure does is find Guile.

AC_MSG_CHECKING([with_guile])
AC_ARG_WITH([guile],
       [AS_HELP_STRING([--with-guile@<:@=GUILE@:>@],
	 [Guile 3 interpreter for the Guile-hosted Oaklisp (default: search for guile3.0, guile)])],,
       [with_guile=check])
AC_MSG_RESULT([$with_guile])
AS_CASE([${with_guile}],
 [check|yes],[AC_PATH_PROGS([GUILE], [guile3.0 guile-3.0 guile], [no])],
 [no],[GUILE=no],
 [GUILE=${with_guile}])
AS_IF([test x${GUILE} != xno],
 [AC_MSG_CHECKING([whether ${GUILE} is Guile 3])
  AS_IF([${GUILE} -c '(exit (if (>= (string->number (major-version)) 3) 0 1))' >/dev/null 2>&1],
	[AC_MSG_RESULT([yes])],
	[AC_MSG_RESULT([no])
	 AS_IF([test x${with_guile} != xcheck],
	       [AC_MSG_ERROR([${GUILE} is not Guile 3])])
	 GUILE=no])])
AC_SUBST([GUILE])


AS_IF([test x${enable_64_bit} = xyes],
      [eight_if_allowed=8],
      [eight_if_allowed=6uuBsNCtBTXDAYKXc9i])

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
AS_CASE([${ac_cv_sizeof_void_p}],
 [4],[AC_MSG_NOTICE([pointer size matches expectations])],
 [${eight_if_allowed}],[AC_MSG_NOTICE([using native 64-bit mode])],
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
      CFLAGS="${CFLAGS} ${flag32}"
      : LDFLAGS="${LDFLAGS} ${flag32}"
      unset ac_cv_sizeof_void_p
      AC_CHECK_SIZEOF([void *])
      AS_CASE([${ac_cv_sizeof_void_p}],
       [0],[AC_MSG_ERROR([bad flags, Indy])],
       [4],[AC_MSG_NOTICE([32-bit memory model achieved])],
	   [AC_MSG_WARN([bad news, pointers are now ${ac_cv_sizeof_void_p} bytes])])])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_SED
AC_CHECK_PROGS([INDENT],[indent],[true])
AS_IF([test x${enable_docs} = xyes],
 [AC_CHECK_PROGS([LATEXMK],[latexmk],[false])])

# Checks for libraries.
# On Windows (MSVC), threads are in kernel32 -- no -lpthread needed.
# On POSIX (including MinGW with winpthreads), link -lpthread.
AS_IF([test x$enable_threads = xyes],
 [AC_SEARCH_LIBS([pthread_create],[pthread],[],
   [AC_MSG_WARN([pthread_create not found; threads may use Windows native API])])])

# Checks for header files.
AC_CHECK_HEADERS([stddef.h stdlib.h string.h sys/time.h unistd.h],,
 [AC_MSG_ERROR([required header file unavailable])])
# Optional headers for portability detection.
AC_CHECK_HEADERS([stdatomic.h windows.h],,,)
# Optional headers for portability detection.
AC_CHECK_HEADERS([stdatomic.h windows.h],,,)

# Checks for typedefs, structures, and compiler characteristics.
AX_CFLAGS_WARN_ALL
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AX_C_LONG_LONG
AC_TYPE_SIZE_T
AC_C_BIGENDIAN
AC_CHECK_TYPES([__int128])

# The architecture of the emulator being built, as Oaklisp sees it:
# the number of 16-bit instructions packed into each reference (bc2),
# the byte order (el or eb), and the reference size in bits.  This
# names the subdirectory of prebuilt/src/world/ holding a world image
# for this machine.  Bytecode is independent of byte order and named
# bc2-32 or bc2-64; bytecode compiled for 32-bit fixnums is also
# usable on a 64-bit machine.  See src/world/architecture.oak.

AS_CASE([${ac_cv_c_bigendian}_${ac_cv_sizeof_void_p}],
 [yes_4],[oak_endian=eb oak_word_size=32],
 [no_4],[oak_endian=el oak_word_size=32],
 [yes_8],[oak_endian=eb oak_word_size=64],
 [no_8],[oak_endian=el oak_word_size=64],
	[AC_MSG_ERROR([unsupported endianity/pointer size combination])])

OAK_INSTRS_PER_REF=2
OAK_HOST_ARCH=bc${OAK_INSTRS_PER_REF}-${oak_endian}${oak_word_size}
OAK_BYTECODE_ARCH=bc${OAK_INSTRS_PER_REF}-${oak_word_size}
AC_MSG_NOTICE([host architecture ${OAK_HOST_ARCH}, bytecode architecture ${OAK_BYTECODE_ARCH}])
AC_SUBST([OAK_HOST_ARCH])
AC_SUBST([OAK_BYTECODE_ARCH])

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
                 src/world/system-version.oak
                 tests/Makefile])
AC_OUTPUT
