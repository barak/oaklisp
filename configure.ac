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
       [AS_HELP_STRING([--enable-threads],[enable thread support (default=no)])],,
       [enable_threads=no])
AC_MSG_RESULT([$enable_threads])
AM_CONDITIONAL([ENABLE_THREADS], [test x${enable_threads} = xyes])

# Bootstrapping.
#
# Oaklisp is written in Oaklisp, so building the world from source
# requires a running Oaklisp: an emulator plus a world image.  There
# are two ways to bootstrap:
#
#  * From a world: a working oakworld.bin (typically the prebuilt one
#    for this architecture in prebuilt/src/world/ARCH/, or an
#    installed one) is used to compile the .oak sources to .oa
#    bytecode and to link the cold world with tool.oak.
#
#  * From bytecode: prebuilt .oa files (prebuilt/src/world/BCARCH/)
#    are linked into a cold world by oak-cold-linker, a C program,
#    which needs no running Oaklisp at all.
#
# Either way, the resulting cold world is booted and the remaining
# layers loaded using the newly built emulator.

AC_MSG_CHECKING([with_world])
AC_ARG_WITH([world],
       [AS_HELP_STRING([--with-world@<:@=WORLD@:>@],
	 [bootstrap from this oakworld.bin (default: search prebuilt/ and installed locations; "no" to bootstrap from prebuilt bytecode)])],,
       [with_world=check])
AC_MSG_RESULT([$with_world])

AC_MSG_CHECKING([with_oaklisp])
AC_ARG_WITH([oaklisp],
       [AS_HELP_STRING([--with-oaklisp=OAKLISP],
	 [existing oaklisp emulator to run the bootstrap world with (default: the one being built)])],,
       [with_oaklisp=no])
AC_MSG_RESULT([$with_oaklisp])

AC_MSG_CHECKING([with_bytecode])
AC_ARG_WITH([bytecode],
       [AS_HELP_STRING([--with-bytecode@<:@=DIR@:>@],
	 [directory of prebuilt .oa bytecode files to bootstrap from when no world is available (default: search prebuilt/)])],,
       [with_bytecode=check])
AC_MSG_RESULT([$with_bytecode])

AS_IF([test x${with_oaklisp} = xyes],
      [AC_PATH_PROG([with_oaklisp], [oaklisp], [no])
       AS_IF([test x${with_oaklisp} = xno],
	     [AC_MSG_ERROR([no oaklisp executable found])])],
      [test x${with_oaklisp} != xno],
      [AS_IF([test -x "${with_oaklisp}"],
	     [with_oaklisp=`cd $(dirname ${with_oaklisp}) && pwd`/$(basename ${with_oaklisp})],
	     [AC_MSG_ERROR([${with_oaklisp} is not an executable])])])

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
AS_IF([test x$enable_threads = xyes],
 [AC_SEARCH_LIBS([pthread_create],[pthread])])

# Checks for header files.
AC_CHECK_HEADERS([stddef.h stdlib.h string.h sys/time.h unistd.h],,
 [AC_MSG_ERROR([required header file unavailable])])

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
AS_IF([test x${oak_word_size} = x64],
      [oak_compatible_bytecode="bc${OAK_INSTRS_PER_REF}-64 bc${OAK_INSTRS_PER_REF}-32"],
      [oak_compatible_bytecode="bc${OAK_INSTRS_PER_REF}-32"])
AC_MSG_NOTICE([host architecture ${OAK_HOST_ARCH}, bytecode architecture ${OAK_BYTECODE_ARCH}])
AC_SUBST([OAK_HOST_ARCH])
AC_SUBST([OAK_BYTECODE_ARCH])

# Determine the architecture a world image is for, from its header
# line or, for legacy worlds, its magic bytes (whose byte order is
# unknown, so it is assumed to be ours).  Sets oak_world_arch to a
# name like bc2-el64, or to "unknown".
oak_world_arch_of() {
  oak_world_arch=unknown
  oak_world_legacy=yes
  AS_CASE([`head -c 1 "$][1" 2>/dev/null | od -An -tx1 | tr -d ' '`],
    [3b],[oak_hdr=`head -n 1 "$][1"`
	  oak_world_legacy=no
	  AS_CASE([$oak_hdr],[*oaklisp-world*],
	    [oak_ws=`expr "$oak_hdr" : '.*word-size=\([[0-9]]*\)'`
	     oak_ipr=`expr "$oak_hdr" : '.*instructions-per-ref=\([[0-9]]*\)'`
	     AS_CASE([$oak_hdr],
	       [*endian=little*],[oak_e=el],
	       [*endian=big*],[oak_e=eb],
	       [oak_e=${oak_endian}])
	     oak_world_arch=bc${oak_ipr:-2}-${oak_e}${oak_ws:-0}])],
    [02],[oak_world_arch=bc2-${oak_endian}32],
    [04],[oak_world_arch=bc2-${oak_endian}64])
}

# The architecture of the emulator that will run the bootstrap world.
AS_IF([test x${with_oaklisp} != xno],
      [oak_bootstrap_arch=`${with_oaklisp} --version 2>/dev/null | sed -n 's/.*(\(bc[[0-9]]*-e[[bl]][[0-9]]*\)).*/\1/p'`
       AS_IF([test -z "${oak_bootstrap_arch}"],
	     [AC_MSG_WARN([cannot determine architecture of ${with_oaklisp}; assuming ${OAK_HOST_ARCH}])
	      oak_bootstrap_arch=${OAK_HOST_ARCH}])],
      [oak_bootstrap_arch=${OAK_HOST_ARCH}])

# Look for a world to bootstrap from.
AS_IF([test x${with_world} = xcheck || test x${with_world} = xyes],
 [AC_MSG_CHECKING([for a ${oak_bootstrap_arch} world to bootstrap from])
  found_world=
  for f in ${srcdir}/prebuilt/src/world/${oak_bootstrap_arch}/oakworld.bin \
	   ${HOME}/lib/oaklisp/oakworld.bin \
	   /usr/local/lib/oaklisp/oakworld.bin \
	   /usr/lib/oaklisp/oakworld.bin \
	   /usr/lib/*/oaklisp/oakworld.bin \
	   /lib/oaklisp/oakworld.bin \
	   /lib/*/oaklisp/oakworld.bin; do
    AS_IF([test -f "$f"],
	  [oak_world_arch_of "$f"
	   AS_IF([test x${oak_world_arch} = x${oak_bootstrap_arch}],
		 [found_world=$f; break])])
  done
  AS_IF([test -n "${found_world}"],
	[AC_MSG_RESULT([${found_world}])
	 with_world=`cd $(dirname ${found_world}) && pwd`/$(basename ${found_world})],
	[AC_MSG_RESULT([none])
	 AS_IF([test x${with_world} = xyes],
	       [AC_MSG_ERROR([--with-world=yes specified but no suitable oakworld.bin found])])
	 with_world=no])],
 [test x${with_world} != xno],
 [AS_IF([test -f "${with_world}"],,[AC_MSG_ERROR([world ${with_world} not found])])
  with_world=`cd $(dirname ${with_world}) && pwd`/$(basename ${with_world})
  oak_world_arch_of "${with_world}"
  AS_IF([test x${oak_world_arch} != x${oak_bootstrap_arch} && test x${oak_world_arch} != xunknown],
	[AC_MSG_ERROR([world ${with_world} is for ${oak_world_arch} but the bootstrap emulator is ${oak_bootstrap_arch}])])])

# Or for prebuilt bytecode.
AS_IF([test x${with_bytecode} = xcheck || test x${with_bytecode} = xyes],
 [AC_MSG_CHECKING([for prebuilt ${OAK_BYTECODE_ARCH} bytecode])
  found_bytecode=
  for a in ${oak_compatible_bytecode}; do
    AS_IF([test -f "${srcdir}/prebuilt/src/world/$a/kernel0.oa"],
	  [found_bytecode=${srcdir}/prebuilt/src/world/$a; break])
  done
  AS_IF([test -n "${found_bytecode}"],
	[AC_MSG_RESULT([${found_bytecode}])
	 with_bytecode=`cd ${found_bytecode} && pwd`],
	[AC_MSG_RESULT([none])
	 AS_IF([test x${with_bytecode} = xyes],
	       [AC_MSG_ERROR([--with-bytecode=yes specified but no prebuilt bytecode found])])
	 with_bytecode=no])],
 [test x${with_bytecode} != xno],
 [AS_IF([test -f "${with_bytecode}/kernel0.oa"],,
	[AC_MSG_ERROR([no bytecode found in ${with_bytecode}])])
  with_bytecode=`cd ${with_bytecode} && pwd`])

# Decide.
AS_IF([test x${with_world} != xno],
 [oak_bootstrap=world
  OAKLISP=${with_oaklisp}
  AS_IF([test x${OAKLISP} = xno], [OAKLISP='$(abs_top_builddir)/src/emulator/oaklisp'])
  OAKWORLDFLAGS="--world ${with_world}"
  # Does the bootstrap system understand --target?  Old worlds (those
  # without a header line) do not, but as they generate the same
  # bytecode this only matters when the bootstrap system has a
  # different word size.  When we have an emulator to run the world
  # with, just try it.
  AC_MSG_CHECKING([whether the bootstrap system supports --target])
  AS_IF([test x${with_oaklisp} != xno],
	[AS_IF([${with_oaklisp} --batch ${OAKWORLDFLAGS} -- --target ${OAK_BYTECODE_ARCH} --exit >/dev/null 2>&1],
	       [oak_target_ok=yes], [oak_target_ok=no])],
	[oak_world_arch_of "${with_world}"
	 AS_IF([test x${oak_world_legacy} = xno], [oak_target_ok=yes], [oak_target_ok=no])])
  AC_MSG_RESULT([${oak_target_ok}])
  AS_IF([test x${oak_target_ok} = xyes],
	[OAKTARGETFLAGS="--target ${OAK_BYTECODE_ARCH}"],
	[OAKTARGETFLAGS=
	 AS_IF([test x${oak_bootstrap_arch} != x${OAK_HOST_ARCH}],
	       [AC_MSG_WARN([bootstrap system cannot cross-compile; results may differ from a native build])])])
  AC_MSG_NOTICE([bootstrapping from world ${with_world} using ${OAKLISP}])],
 [test x${with_bytecode} != xno],
 [oak_bootstrap=bytecode
  OAKLISP='$(abs_top_builddir)/src/emulator/oaklisp'
  OAKWORLDFLAGS=
  OAKTARGETFLAGS=
  AC_MSG_NOTICE([bootstrapping from prebuilt bytecode in ${with_bytecode} using oak-cold-linker])],
 [AC_MSG_ERROR([no way to bootstrap: no world image and no prebuilt bytecode found (see --with-world and --with-bytecode)])])

AC_SUBST([OAKLISP])
AC_SUBST([OAKWORLDFLAGS])
AC_SUBST([OAKTARGETFLAGS])
AC_SUBST([PREBUILT_BYTECODE_DIR], [${with_bytecode}])
AM_CONDITIONAL([BOOTSTRAP_FROM_WORLD], [test x${oak_bootstrap} = xworld])

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
