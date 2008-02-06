// This file is part of Oaklisp.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// The GNU GPL is available at http://www.gnu.org/licenses/gpl.html
// or from the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA


/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/


/*
 *  Some configuration parameters explained:
 *  ========================================
 *
 *  ASHR2
 *  Must do arithmetic right shift on its argument.
 *  Use ((x)/4) if your compiler generates logical shifts for
 *  ((x)>>2)
 *
 *
 *  UNALIGNED_MALLOC
 *  Defined if malloc() might return a pointer that is not longword
 *  aligned, i.e. whose low two bits might not be 0.
 *
 *
 *  THREADS
 *  If defined, heavyweight OS pthreads are enabled.
 *
 */

#ifndef _CONFIG_H_INCLUDED
#define _CONFIG_H_INCLUDED

#if defined(linux) && defined (__GNUC__)
/*** Linux with GCC ***/

#include <bits/wordsize.h>
#if (__WORDSIZE != 32)
#error word size must be 32 bits
#endif

#define ASHR2(x) ((x)>>2)
#define HAVE_GETRUSAGE
//#define THREADS
#ifdef THREADS
#define MAX_THREAD_COUNT 200
#endif
#include <unistd.h>		/* for the chdir() and isatty() functions */

#elif defined(linux) && defined(__arm__)
/*** Linux on Arm target ***/

#define WORDSIZE 32
#define HAVE_LONG_LONG
#define ASHR2(x) ((x)>>2)
#define BYTE_GENDER little_endian
#define HAVE_GETRUSAGE

#include <unistd.h>            /* for the chdir() and isatty() functions */

else
/*** no machine specified ***/

#error must edit config.h

#endif

/* Speed parameters */

/* Turn off most runtime debugging features that slow down the system. */
// #define FAST

/* Toggle specific optimizations. */

/* Activate operation-method association list move-to-front. */
#ifndef THREADS
#define OP_METH_ALIST_MTF
#endif

/* Activate operation-type method cache. */
#ifndef THREADS
#define OP_TYPE_METH_CACHE
#endif

#ifdef USING_HORRIBLE_MS_WINDOWS
typedef unsigned long  u_int32_t;
typedef unsigned short u_int16_t;
typedef	int	int32_t;
typedef	unsigned char u_int8_t;
typedef signed char int8_t;
typedef	short	int16_t;
#endif // USING_HORRIBLE_MS_WINDOWS

#define	inline

#endif
