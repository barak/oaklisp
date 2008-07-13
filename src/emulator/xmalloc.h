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

#ifndef _XMALLOC_H_INCLUDED
#define _XMALLOC_H_INCLUDED

#include <stddef.h>
#include "data.h"

extern bool isaligned(void *x);
extern void *xmalloc(size_t size);
extern void alloc_space(space_t * pspace, size_t size_requested);
extern void free_space(space_t * pspace);
extern void realloc_space(space_t * pspace, size_t size_requested);
char *oak_c_string(ref_t * oakstr, int len);

#endif
