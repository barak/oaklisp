/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/


/*
 *  Some configuration parameters explained:
 *  ========================================
 *
 *  WORDSIZE
 *  Size of the 'long integer' datatype in bits, must be less than or
 *  equal to the size of the '(char*)' type.
 *
 *  ASHR2
 *  Must do arithmetic right shift on its argument.
 *  Use ((x)/4) if your compiler generates logical shifts for
 *  ((x)>>2)
 *
 *  
 *  BYTE_GENDER
 *  is 'little_endian' or 'big_endian' depending on your machine.
 *  (Some parts of the code need to know the endianity.)
 *  
 *
 *  HAVE_LONG_LONG
 *  Some machines have a 64-bit variant of an integer called a 
 *  "long long", which makes overflow detection easier.
 *
 *
 *  UNALIGNED_MALLOC
 *  Defined if malloc() might return a pointer that is not longword
 *  aligned, i.e. whose low two bits might not be 0.
 *
 *  THREADS
 *  If defined, heavyweight OS pthreads are enabled.
 *
 */

#ifndef _CONFIG_H_INCLUDED
#define _CONFIG_H_INCLUDED


/* Speed parameters */

/* Turn off most runtime debugging features that slow down the system. */
// #define FAST

/* Toggle specific optimizations. */

/* Activate operation-method association list move-to-front. */
#define OP_METH_ALIST_MTF

/* Activate operation-type method cache. */
#define OP_TYPE_METH_CACHE


#if defined(linux) && defined (__GNUC__) && defined(i386)
/*** Linux with GCC on Intel target ***/

#define WORDSIZE 32
#define HAVE_LONG_LONG
#define ASHR2(x) ((x)>>2)
#define BYTE_GENDER little_endian
#define HAVE_GETRUSAGE
#define THREADS
#define MAX_THREAD_COUNT 200

#include <unistd.h>		/* for the chdir() and isatty() functions */

#elif defined(sun) && defined(__GNUC__)
/*** Sun with GCC ***/

#define WORDSIZE 32
/*#define HAVE_LONG_LONG */
#define ASHR2(x) ((x)>>2)
#define BYTE_GENDER big_endian
#define HAVE_GETRUSAGE

#include <unistd.h>		/* for the chdir() and isatty() functions */

#elif defined(_MSC_VER)
#if defined(_M_IX86) && (_MSC_VER >= 1100)
/*** Visual C++ 5.0 or later on 32-bit Intel target ***/

#define WORDSIZE 32
#define ASHR2(x) ((x)>>2)
#define BYTE_GENDER little_endian
#define PATH_SEPARATOR_CHAR '\\'

/* the following is for high-precision timing */
#define _WIN32_WINNT 0x0400
#include <windows.h>
/* #include <process.h> */
#define HAVE_GETTICKCOUNT

/* the following is for isatty(), fileno() */
#include <io.h>

/* the following is for chdir() */
#include <direct.h>

#endif

#elif (defined(__WINDOWS_386__) || defined(__NT__)) \
        && defined (__WATCOMC__) && !defined(__DOS__)
/*** Watcom C++ on 32-bit Windows ***/

#define WORDSIZE 32
#define ASHR2(x) ((x)>>2)
#define BYTE_GENDER little_endian
#define PATH_SEPARATOR_CHAR '\\'

#include <windows.h>
#define HAVE_GETTICKCOUNT

#include <direct.h>		/* for the chdir() function */
#include <io.h>			/* for the isatty(), fileno() functions */

#elif defined(__DOS__) &&  defined(__386__) && defined (__WATCOMC__)
/*** Watcom C++ on 32-bit Extended-DOS ***/


#define WORDSIZE 32
#define ASHR2(x) ((x)>>2)
#define BYTE_GENDER little_endian
#define PATH_SEPARATOR_CHAR '\\'

#include <sys\types.h>
#include <direct.h>		/* for the chdir() function */
#include <io.h>			/* for the isatty(), fileno() functions */

#elif defined(_ICC)
/*** Inmos C Transputer Development Toolkit ***/

#define WORDSIZE 32
#define ASHR2(x) ((x)>>2)
  /* note: to get arithmetic right shifts with the inmos compiler
   * caution: you must compile with the /FS option! */
#define BYTE_GENDER little_endian
#define PATH_SEPARATOR_CHAR '\\'

#define ISATTY(stream) 1
#define chdir(x) (-1)


#elif defined(vax) && defined(decc)
/*** DEC Vax and decc ***/
#error vax not yet ported


#elif defined(AMIGA)
/*** Amiga target ***/

#define WORDSIZE 32
#define ASHR2(x) ((x)>>2)
#define BYTE_GENDER big_endian

#if defined(__GNUC__)		/* using either libnix or ixemul */
#define HAVE_LONG_LONG
#include <unistd.h>		/* for the chdir() and isatty() functions */
#elif defined(_DCC)		/* DICE (2.06.nn) */
#include <fcntl.h>		/* for isatty().  chdir() is in stdio.h */
#endif


#else
/*** no machine specified ***/
#error must edit config.h
#endif

#endif
