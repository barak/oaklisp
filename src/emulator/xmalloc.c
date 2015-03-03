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
  void *ptr = (void *)pspace->start;
  void *newptr = realloc(ptr, sizeof(ref_t) * (size_requested));

  if (ptr == NULL)
    {
      fprintf(stderr, "error: realloc_space() does not expect a null pointer\n");
      exit(EXIT_FAILURE);
    }

  /* This is called during a full GC to convert the old new space to
     the new spatic space.  Any unallocated new space is trimmed.  So
     this should be decreasing the size, or at worst leaving it the
     same.  For that reason we do not expect the storage to be moved.
     If it is: uh oh! */

  if (ptr != newptr) 
    {
      fprintf(stderr, "error: realloc() with decreased size moved storage in realloc_space()\n");
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
   */
  char *const cstring = xmalloc(len + 1);
  oak_c_string_fill(oakstr, cstring, len);
  return cstring;
}
