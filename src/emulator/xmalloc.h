/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#ifndef _XMALLOC_H_INCLUDED
#define _XMALLOC_H_INCLUDED

#include <stddef.h>
#include "data.h"

extern bool isaligned(void *x);
extern void *xmalloc (size_t size);
extern void alloc_space (space_t * pspace, size_t size_requested);
extern void free_space (space_t * pspace);
extern void realloc_space (space_t * pspace, size_t size_requested);
char *oak_c_string(ref_t * oakstr, int len);

#endif
