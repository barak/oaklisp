/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "config.h"
#include "data.h"
#include "xmalloc.h"



bool
isaligned(void *x)
{
  return ((unsigned long)x & 3) == 0;
}



void *
xmalloc (size_t size)
{
  /* replacement for ansi-library-malloc */
  void *ptr = malloc (size);
  if (ptr)
    {
      /*
         #ifndef UNALIGNED_MALLOC
         assert(isaligned(ptr));
         #endif
       */
      return ptr;
    }
  else
    {
      fprintf (stderr,
	       "ERROR(xmalloc): Unable to allocate %lu bytes.\n",
	       (unsigned long) size);
      exit (EXIT_FAILURE);
      return 0;
    }
}

void
alloc_space (space_t * pspace, size_t size_requested)
{
  /* size_requested measures references */

#ifdef UNALIGNED_MALLOC
  void *ptr = xmalloc (sizeof (ref_t) * (size_requested + 1));

  pspace->displacement = (size_t) ((unsigned long) ptr & (3ul));
  pspace->start = (ref_t *) (((unsigned long) ptr + 3) & ~3ul);

/*  Explanation:
   displacement address correction
   0 (mod 4)            + 0, which is good, to preserve higher alignment
   1 (mod 4)       + 3
   2 (mod 4)       + 2
   3 (mod 4)       + 1
   we waste maximum 4 bytes
 */

#else /* UNALIGNED_MALLOC */
  void *ptr = xmalloc (sizeof (ref_t) * size_requested);
  pspace->start = (ref_t *) ptr;

#endif
  pspace->size = size_requested;
  pspace->end = pspace->start + size_requested;

}


void
free_space (space_t * pspace)
{
  void *ptr;
#ifdef UNALIGNED_MALLOC
  if (pspace.displacement)
    {				/* reverse alignment correction */
      ptr = (void *) ((unsigned long) pspace->start - 4
		      + pspace->displacement);
    }
  else
    {
      ptr = (void *) pspace->start;
    }
#else /* UNALIGNED_MALLOC */
  ptr = (void *) pspace->start;
#endif
  assert (ptr != 0);
  free (ptr);
  pspace->start = pspace->end = 0;
  pspace->size = 0;
#ifdef UNALIGNED_MALLOC
  pspace->displacement = 0;
#endif
}

void
realloc_space (space_t * pspace, size_t size_requested)
{
  void *ptr;
#ifdef UNALIGNED_MALLOC
  if (pspace->displacement)
    {				/* reverse alignment correction */
      ptr = (void *) ((unsigned long) pspace->start - 4
		      + pspace->displacement);
      realloc (ptr, sizeof (ref_t) * (size_requested + 1));
    }
  else
    {
      ptr = (void *) pspace->start;
      realloc (ptr, sizeof (ref_t) * (size_requested));
      /* we need not waste another 4 bytes here */
    }

#else /* UNALIGNED_MALLOC */
  ptr = (void *) pspace->start;
  realloc (ptr, sizeof (ref_t) * (size_requested));

#endif
  if (ptr)
    {
      pspace->end = pspace->start + size_requested;
      pspace->size = size_requested;
    }
  else
    {
      fprintf (stderr,
	       "(ERROR(realloc_space): Unable to reallocate %lu bytes.\n",
	       (unsigned long) size_requested);
      exit (EXIT_FAILURE);
    }
}



char *
oak_c_string(ref_t * oakstr, int len)
{
  /* Converts an Oaklisp string, given by a pointer to its
     start and a length, to an equivalent C-string.
     The storage allocated by this routine must be free()-ed.
  */
  int i = 0;
  unsigned char *const cstring = xmalloc (len + 1);
  unsigned long temp;
  unsigned char *const cp = (unsigned char *) &temp;

  while (i + 2 < len)
    {
      temp = (unsigned long) *oakstr >> 2;
      cstring[i + 0] = cp[0];
      cstring[i + 1] = cp[1];
      cstring[i + 2] = cp[2];
      oakstr++;
      i += 3;
    }
  if (i + 1 < len)
    {
      temp = (unsigned long) *oakstr >> 2;
      cstring[i + 0] = cp[0];
      cstring[i + 1] = cp[1];
      oakstr++;
      i += 2;
    }
  else if (i < len)
    {
      temp = (unsigned long) *oakstr >> 2;
      cstring[i + 0] = cp[0];
      /* oakstr++; */
      i++;
    }
  cstring[i + 0] = '\0';
  return cstring;
}
