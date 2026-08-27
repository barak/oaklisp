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
#include <limits.h>
#include "config.h"
#include "data.h"
#include "xmalloc.h"
#include "gc.h"
#include "weak.h"
#include "threads.h"

#ifdef THREADS
oak_mutex_t wp_lock = OAK_MUTEX_INITIALIZER;
#endif


/*
 * Weak pointers are done with a simple table that goes from weak
 * pointers to objects, and a hash table that goes from objects to
 * their weak pointers.

 * In the future, this will be modified to keep separate hash tables
 * for the different areas, so that objects in spatic space need not
 * be rehashed.

 * Plus another one for unboxed values like fixnums.

 */


/* These are not constant: both tables grow on demand.  See
   grow_wp_tables() below. */

int wp_table_size = 3000;
int wp_hashtable_size = 3017;


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



/* Fibonacci hashing: floor( 2^N * (sqrt(5)-1)/2 ) for N-bit words. */
#if __WORDSIZE == 64
#define wp_key(r) ((unsigned long) 0x9E3779B97F4A7C15UL*(r))
#else
#define wp_key(r) ((unsigned long) 0x9E3779BB*(r))	/* >>10, == 2654435771L */
#endif

void
init_weakpointer_tables(void)
{
  wp_table = (ref_t *) xmalloc((wp_table_size + 1) * sizeof(ref_t));
  wp_hashtable =
    (wp_hashtable_entry *) xmalloc(sizeof(wp_hashtable_entry)
				   * wp_hashtable_size);
}


/* Return a prime at least as large as n, for use as a hash table size. */
static int
next_hash_prime(int n)
{
  int candidate;

  if (n < 17)
    n = 17;
  for (candidate = n | 1;; candidate += 2)
    {
      int d;
      bool composite = false;

      for (d = 3; d <= candidate / d; d += 2)
	if (candidate % d == 0)
	  {
	    composite = true;
	    break;
	  }
      if (!composite)
	return candidate;
    }
}


/* Register r as having weak pointer wp.  The caller guarantees that
   the hash table has at least one free slot, so the probe terminates. */
static void
enter_wp(ref_t r, ref_t wp)
{
  long i = wp_key(r) % wp_hashtable_size;
  long probes;

  for (probes = 0; probes < wp_hashtable_size; probes++)
    {
      if (wp_hashtable[i].obj == e_false)
	{
	  wp_hashtable[i].obj = r;
	  wp_hashtable[i].wp = wp;
	  return;
	}
      if (++i == wp_hashtable_size)
	i = 0;
    }

  /* Unreachable unless the table was allowed to fill up completely. */
  fprintf(stderr,
	  "\nFatal error: the weak pointer hash table is full"
	  " (%d entries).\n", wp_hashtable_size);
  exit(EXIT_FAILURE);
}


/* Rebuild the weak pointer hash table from the information in the table
   that takes weak pointers to objects.  The caller holds wp_lock. */
static void
rebuild_wp_hashtable_locked(void)
{
  long i;

  for (i = 0; i < wp_hashtable_size; i++)
    wp_hashtable[i].obj = e_false;

  for (i = 0; i < wp_index; i++)
    if (wp_table[1 + i] != e_false)
      enter_wp(wp_table[1 + i], INT_TO_REF(i));
}


void
rebuild_wp_hashtable(void)
{
  THREADY(oak_mutex_lock(&wp_lock));
  rebuild_wp_hashtable_locked();
  THREADY(oak_mutex_unlock(&wp_lock));
}


/* Grow both tables so that at least new_table_size weak pointers fit,
   then rehash.  The caller holds wp_lock.  The hash table is kept at
   roughly twice the capacity of the weak pointer table so that the
   open-addressing probe sequence stays short and always terminates. */
static void
grow_wp_tables(int needed)
{
  int new_table_size = wp_table_size;
  ref_t *new_wp_table;
  wp_hashtable_entry *new_hashtable;
  int new_hashtable_size;

  if (new_table_size < 1)
    new_table_size = 3000;
  while (new_table_size < needed)
    {
      if (new_table_size > INT_MAX / 2)
	{
	  fprintf(stderr,
		  "\nFatal error: too many weak pointers (%d requested).\n",
		  needed);
	  exit(EXIT_FAILURE);
	}
      new_table_size *= 2;
    }

  new_hashtable_size = next_hash_prime(2 * new_table_size + 1);

  new_wp_table =
    (ref_t *) realloc(wp_table, (size_t)(new_table_size + 1) * sizeof(ref_t));
  if (new_wp_table == NULL)
    {
      fprintf(stderr,
	      "\nERROR: unable to grow the weak pointer table to %d entries.\n",
	      new_table_size);
      exit(EXIT_FAILURE);
    }
  wp_table = new_wp_table;
  wp_table_size = new_table_size;

  new_hashtable =
    (wp_hashtable_entry *) realloc(wp_hashtable,
				   (size_t)new_hashtable_size
				   * sizeof(wp_hashtable_entry));
  if (new_hashtable == NULL)
    {
      fprintf(stderr,
	      "\nERROR: unable to grow the weak pointer hash table to"
	      " %d entries.\n", new_hashtable_size);
      exit(EXIT_FAILURE);
    }
  wp_hashtable = new_hashtable;
  wp_hashtable_size = new_hashtable_size;

  rebuild_wp_hashtable_locked();
}


/* Make room for at least n entries in the weak pointer table.  Used by
   the world loader, which knows up front how many it needs. */
void
ensure_wp_capacity(int n)
{
  THREADY(oak_mutex_lock(&wp_lock));
  if (n > wp_table_size)
    grow_wp_tables(n);
  THREADY(oak_mutex_unlock(&wp_lock));
}


/* Return weak pointer associated with obj, making a new one if necessary. */

ref_t
ref_to_wp(ref_t r)
{
  long i;
  ref_t temp;
  ref_t result;

  if (r == e_false)
    return INT_TO_REF(-1);

  THREADY(oak_mutex_lock(&wp_lock));
  i = wp_key(r) % wp_hashtable_size;

  while (1)			/* forever */
    {
      temp = wp_hashtable[i].obj;
      if (temp == r)
	{
	  result = wp_hashtable[i].wp;
	  THREADY(oak_mutex_unlock(&wp_lock));
	  return result;
	}
      else if (temp == e_false)
	{
	  /* Make a new weak pointer, installing it in both tables.
	     Grow first if the weak pointer table is full or the hash
	     table is getting crowded; growing rehashes, so the probe
	     has to be restarted afterwards. */
	  if (wp_index >= wp_table_size
	      || 2 * (wp_index + 1) >= wp_hashtable_size)
	    {
	      grow_wp_tables(wp_index + 1);
	      i = wp_key(r) % wp_hashtable_size;
	      while (wp_hashtable[i].obj != e_false)
		if (++i == wp_hashtable_size)
		  i = 0;
	    }
	  wp_hashtable[i].obj = wp_table[1 + wp_index] = r;
	  result = wp_hashtable[i].wp = INT_TO_REF(wp_index++);
	  THREADY(oak_mutex_unlock(&wp_lock));
	  return result;
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
