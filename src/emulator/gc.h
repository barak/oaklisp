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


#ifndef _GC_H_INCLUDED
#define _GC_H_INCLUDED

#include "config.h"
#include "data.h"

extern bool full_gc;
extern void printref(FILE * fd, ref_t refin);

extern void gc(bool pre_dump, bool full_gc, char *reason,
	       size_t amount);

#define GC_MEMORY(v) \
{*gc_examine_ptr++ = (v);}
		/*
		   assert(gc_examine_ptr < &gc_examine_buffer[GC_EXAMINE_BUFFER_SIZE]);\
		   } */

#define GC_RECALL(v) \
{(v) = *--gc_examine_ptr;}
		/*
		   assert(gc_examine_ptr >= gc_examine_buffer);\
		   } */

#ifdef THREADS
extern int gc_ready[];
extern bool gc_pending;
extern pthread_mutex_t gc_lock;
#endif

extern void set_gc_flag (bool flag);
extern int get_next_index();
extern void free_registers();
extern void wait_for_gc();

#endif
