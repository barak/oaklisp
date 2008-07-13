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
#include "config.h"
#include "data.h"
#include "xmalloc.h"
#include "gc.h"
#include "weak.h"


/*
 * Weak pointers are done with a simple table that goes from weak
 * pointers to objects, and a hash table that goes from objects to
 * their weak pointers.

 * In the future, this will be modified to keep separate hash tables
 * for the different areas, so that objects in spatic space need not
 * be rehashed.

 * Plus another one for unboxed values like fixnums.

 */


const int wp_table_size = 3000;
const int wp_hashtable_size = 3017;


ref_t *wp_table;		/* wp -> ref */
int wp_index = 0;		/* number of entries in wp_table */


/* A hash table from references to their weak pointers.  This hash
 * table is not saved in dumped worlds, and is rebuilt from scratch
 * after each GC and upon booting a new world.

 * Structure of this hash table:

 * Keys are references themselves, smashed about and xored if deemed
 * necessary.

 * Sequential rehash, single probe.
 */


typedef struct
  {
    ref_t obj;
    ref_t wp;
  }
wp_hashtable_entry;

wp_hashtable_entry *wp_hashtable;



/* The following magic number is floor( 2^32 * (sqrt(5)-1)/2 ). */
#define wp_key(r) ((unsigned long) 0x9E3779BB*(r))	/* >>10, == 2654435771L */

void
init_weakpointer_tables(void)
{
  wp_table = (ref_t *) xmalloc((wp_table_size + 1) * sizeof(ref_t));
  wp_hashtable =
    (wp_hashtable_entry *) xmalloc(sizeof(wp_hashtable_entry)
				   * wp_hashtable_size);
}


/* Register r as having weak pointer wp. */
static void
enter_wp(ref_t r, ref_t wp)
{
  long i = wp_key(r) % wp_hashtable_size;

  while (1)			/* forever */
    if (wp_hashtable[i].obj == e_false)
      {
	wp_hashtable[i].obj = r;
	wp_hashtable[i].wp = wp;
	return;
      }
    else if (++i == wp_hashtable_size)
      i = 0;
}


/* Rebuild the weak pointer hash table from the information in the table
   that takes weak pointers to objects. */
void
rebuild_wp_hashtable(void)
{
  long i;

  for (i = 0; i < wp_hashtable_size; i++)
    wp_hashtable[i].obj = e_false;

  for (i = 0; i < wp_index; i++)
    if (wp_table[1 + i] != e_false)
      enter_wp(wp_table[1 + i], INT_TO_REF(i));
}


/* Return weak pointer associated with obj, making a new one if necessary. */

ref_t
ref_to_wp(ref_t r)
{
  long i;
  ref_t temp;

  if (r == e_false)
    return INT_TO_REF(-1);
  i = wp_key(r) % wp_hashtable_size;

  while (1)			/* forever */
    {
      temp = wp_hashtable[i].obj;
      if (temp == r)
	{
	  return wp_hashtable[i].wp;
	}
      else if (temp == e_false)
	{
	  /* Make a new weak pointer, installing it in both tables: */
	  wp_hashtable[i].obj = wp_table[1 + wp_index] = r;
	  return wp_hashtable[i].wp = INT_TO_REF(wp_index++);
	}
      else if (++i == wp_hashtable_size)
	{
	  i = 0;
	}
    }
}

#if 0				/* commented out */

#include <stdio.h>
void
wp_hashtable_distribution(void)
{
  long i;

  for (i = 0; i < wp_hashtable_size; i++)
    {
      ref r = wp_hashtable[i].obj;

      if (r == e_false)
	(void)putchar('.');
      else
	{
	  unsigned long j = wp_key(r) % wp_hastable_size;
	  long dist = i - j;

	  if (dist < 0)
	    dist += wp_hastable_size;

	  if (dist < 1 + '9' - '0')
	    (void)putchar((char)('0' + dist));
	  else if (dist < 1 + 'Z' - 'A' + 1 + '9' - '0')
	    (void)putchar((char)('A' + dist - (1 + '9' - '0')));
	  else
	    (void)putchar('*');
	}

      fflush(stdout);
    }
}

#endif /* commented out */


unsigned long
post_gc_wp(void)
{
  /* Scan the weak pointer table.  When a reference to old space is
     found, check if the location has a forwarding pointer.  If so,
     update it; if not, discard it. */
  long i;
  unsigned long discard_count = 0;

  for (i = 0; i < wp_index; i++)
    {
      ref_t r = wp_table[1 + i], *p;

      if ((r & PTR_MASK) && (p = ANY_TO_PTR(r), OLD_PTR(p)))
	{
	  ref_t r1 = *p;

	  if (TAG_IS(r1, LOC_TAG) && NEW_PTR(LOC_TO_PTR(r1)))
	    {
	      wp_table[1 + i] = TAG_IS(r, LOC_TAG) ? r1 : r1 | PTR_TAG;
	    }
	  else
	    {
	      wp_table[1 + i] = e_false;
	      discard_count += 1;
	    }
	}
    }

  rebuild_wp_hashtable();

  return discard_count;
}
