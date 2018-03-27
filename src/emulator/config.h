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
 *  Copyright (c) by Barak A. Pearlmutter and Kevin J. Lang, 1987-99. *
 *  Copyright (c) by Alex Stuebinger, 1998-99.                        *
 *  Distributed under the GNU General Public License v2 or later      *
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
 *  THREADS
 *  If defined, heavyweight OS pthreads are enabled.
 *
 */

#ifndef _CONFIG_H_INCLUDED
#define _CONFIG_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#define ASHR2(x) ((x)>>2)

#ifdef THREADS
#ifndef MAX_THREAD_COUNT
#define MAX_THREAD_COUNT 200
#endif
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

#endif
