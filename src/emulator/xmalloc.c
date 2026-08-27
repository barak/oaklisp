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

#define _REENTRANT

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "config.h"
#include "data.h"
#include "xmalloc.h"




void *
xmalloc(size_t size)
{
  /* replacement for ansi-library-malloc */
  void *ptr = malloc(size);
  if (ptr)
    {
      return ptr;
    }
  else
    {
      fprintf(stderr,
	      "ERROR(xmalloc): Unable to allocate %lu bytes.\n",
	      (unsigned long)size);
      exit(EXIT_FAILURE);
      return 0;
    }
}

void
alloc_space(space_t * pspace, size_t size_requested)
{
  /* size_requested measures references */
  void *ptr = xmalloc(sizeof(ref_t) * size_requested);
  pspace->start = (ref_t *) ptr;

  pspace->size = size_requested;
  pspace->end = pspace->start + size_requested;
}


void
free_space(space_t * pspace)
{
  void *ptr = (void *)pspace->start;
  assert(ptr != 0);
  free(ptr);
  pspace->start = pspace->end = 0;
  pspace->size = 0;
}


/*This is called by gc.  Can't acquire alloc lock from gc
  since inversion occurs with macro ALLOC_SS but no need*/
void
realloc_space(space_t * pspace, size_t size_requested)
{
  /* This is called during a full GC to convert the old new space into
     the new spatic space.  Any unallocated tail is trimmed.

     The live world holds absolute pointers into this block, so it must
     not move.  realloc() gives no such guarantee even when shrinking
     (glibc's mremap path for large mmap'ed chunks is free to relocate,
     and ASan relocates unconditionally), so we do not call it at all:
     we simply keep the oversized malloc block and shrink the space's
     logical extent.  The tail is wasted until the space is freed, which
     is always safe. */

  if (pspace->start == NULL)
    {
      fprintf(stderr, "error: realloc_space() does not expect a null pointer\n");
      exit(EXIT_FAILURE);
    }

  if (size_requested > pspace->size)
    {
      fprintf(stderr, "error: realloc_space() cannot grow a space in place"
	      " (%lu -> %lu refs)\n",
	      (unsigned long)pspace->size, (unsigned long)size_requested);
      exit(EXIT_FAILURE);
    }

  pspace->end = pspace->start + size_requested;
  pspace->size = size_requested;
}




void
oak_c_string_fill(ref_t * oakstr, char *cstring, int len)
{
  int i = 0;

  while (i + 2 < len)
    {
      unsigned long temp = *oakstr;
      cstring[i + 0] = 0xff & (temp >> 2);
      cstring[i + 1] = 0xff & (temp >> (8 + 2));
      cstring[i + 2] = 0xff & (temp >> (16 + 2));
      oakstr++;
      i += 3;
    }
  if (i + 1 < len)
    {
      unsigned long temp = *oakstr;
      cstring[i + 0] = 0xff & (temp >> 2);
      cstring[i + 1] = 0xff & (temp >> (8 + 2));
      oakstr++;
      i += 2;
    }
  else if (i < len)
    {
      unsigned long temp = *oakstr;
      cstring[i + 0] = 0xff & (temp >> 2);
      /* oakstr++; */
      i++;
    }
  cstring[i + 0] = '\0';
}



char *
oak_c_string(ref_t * oakstr, int len)
{
  /* Converts an Oaklisp string, given by a pointer to its
     start and a length, to an equivalent C-string.
     The storage allocated by this routine must be free()-ed.

     The length comes from a value on the Oaklisp stack, so it is not
     necessarily sane.  A negative one would ask xmalloc for fewer than
     the two bytes oak_c_string_fill goes on to touch, so clamp it: an
     empty C string is the sensible reading of "no characters".
   */
  char *cstring;

  if (len < 0)
    len = 0;
  cstring = xmalloc((size_t)len + 1);
  oak_c_string_fill(oakstr, cstring, len);
  return cstring;
}
