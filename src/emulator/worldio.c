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


/***********************************************************************
 * Copyright (c) by Barak A. Pearlmutter and Kevin J. Lang, 1987-2000. *
 * Copyright (c) by Alex Stuebinger, 1998-99.                          *
 * Distributed under the GNU General Public License v2 or later        *
 ***********************************************************************/

#define _REENTRANT

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "config.h"
#include "data.h"
#include "xmalloc.h"
#include "worldio.h"
#include "weak.h"


void xfread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  size_t r = fread(ptr, size, nmemb, stream);
  if (r != nmemb) 
    {
      fprintf(stderr,
	      "error: expected to read %lu elements of size %lu but received %lu\n",
	      (unsigned long)nmemb, (unsigned long)size, (unsigned long)r);
      exit(EXIT_FAILURE);
    }
}


/*
 * Format of Oaklisp world image:
 *
 * UNUSED: <size of value stack>
 * UNUSED: <size of context stack>
 * <reference to method for booting>
 * <number of words to load>
 *
 * <words to load>
 *
 * <size of weak pointer table>
 * <contents of weak pointer table>
 */


bool input_is_binary;


/* These are for making the world zero-based and contiguous in dumps. */

static ref_t
contig(ref_t r, bool just_new)
{
  ref_t *p = ANY_TO_PTR(r);

  if (just_new)
    if (NEW_PTR(p))
      return ((ref_t) (p - new_space.start) << 2) | (r & 3);
    else
      printf("Non-new pointer %lu found.\n", (unsigned long)r);
  else if (SPATIC_PTR(p))
    return ((ref_t) (p - spatic.start) << 2) | (r & 3);
  else if (NEW_PTR(p))
    return ((ref_t) (p - new_space.start + spatic.size) << 2) | (r & 3);
  else
    printf("Non-new or spatic pointer %lu found.\n", (unsigned long)r);
  return r;
}

#define contigify(r) ((r)&0x2 ? contig((r),just_new) : (r))
#define CONTIGIFY(v) { if ((v)&2) (v) = contig((v),just_new); }


static ref_t
read_ref(FILE * d)
{
  /* Read a reference from a file: */
  int c;

  /* It's easy to read a reference from a binary file. */
  if (input_is_binary)
    {
      ref_t a;
      xfread((void *)&a, sizeof(ref_t), 1, d);
      return a;
    }
  else
    {
      ref_t a;
      unsigned long b;
      fscanf(d, " ");
      bool swapem = (c = getc(d)) == '^';
      if (!swapem) ungetc(c, d);
      if (fscanf(d, "%lx", &b) != 1)
	{
	  printf("Error reading cold load file, might be truncated.\n");
	  exit(EXIT_FAILURE);
	}
      a = b;
#ifndef WORDS_BIGENDIAN
      if (swapem)
	a = (a << 16 | a >> 16);
#endif
      return a;
    }				/* input_is_binary */
}


#define REFBUFSIZ 256

ref_t refbuf[REFBUFSIZ];

static void
dump_binary_world(bool just_new)
{
  FILE *wfp = 0;
  ref_t *memptr;
  ref_t theref;

  /* CAUTION: STACK SPACE!!! */

  int imod = 0;
  unsigned long worlsiz = free_point - new_space.start;
  unsigned long DUMMY = 0;

  fprintf(stderr, "Dumping in binary.\n");

  wfp = fopen(dump_file_name, WRITE_BINARY_MODE);
  if (!wfp)
    {
      fprintf(stderr, "error opening \"%s\"\n", dump_file_name);
      exit(EXIT_FAILURE);
    }

  if (!just_new)
    worlsiz += spatic.size;

  putc('\002', wfp);
  putc('\002', wfp);
  putc('\002', wfp);
  putc('\002', wfp);

  /* Header information. */
  fwrite((const void *)&DUMMY, sizeof(ref_t), 1, wfp);
  fwrite((const void *)&DUMMY, sizeof(ref_t), 1, wfp);
  theref = contigify(e_boot_code);
  fwrite((const void *)&theref, sizeof(ref_t), 1, wfp);
  fwrite((const void *)&worlsiz, sizeof(ref_t), 1, wfp);

  /* Dump the heap. */
  /* Maybe dump spatic space. */
  if (!just_new)
    for (memptr = spatic.start; memptr < spatic.end; memptr++)
      {
	theref = *memptr;
	CONTIGIFY(theref);
	refbuf[imod++] = theref;
	if (imod == REFBUFSIZ)
	  {
	    fwrite((const void *)refbuf, sizeof(ref_t), imod, wfp);
	    imod = 0;
	  }
      }
  /* Dump new space. */
  for (memptr = new_space.start; memptr < free_point; memptr++)
    {
      theref = *memptr;
      CONTIGIFY(theref);
      refbuf[imod++] = theref;
      if (imod == REFBUFSIZ)
	{
	  fwrite((const void *)refbuf, sizeof(ref_t), imod, wfp);
	  imod = 0;
	}
    }
  if (imod != 0)
    fwrite((const void *)refbuf, sizeof(ref_t), imod, wfp);


  /* Weak pointer table. */
  theref = (ref_t) wp_index;
  fwrite((const void *)&theref, sizeof(ref_t), 1, wfp);

  for (imod = 0; imod < wp_index; imod++)
    {
      theref = wp_table[1 + imod];
      CONTIGIFY(theref);
      fwrite((const void *)&theref, sizeof(ref_t), 1, wfp);
    }

  fclose(wfp);
}


static void
dump_ascii_world(bool just_new)
{
  ref_t *memptr, theref;
  long i;
  int eighter = 0;
  char *control_string = (dump_base == 10 ? "%ld " : "%lx ");
  FILE *wfp = 0;

  fprintf(stderr, "Dumping in ascii.\n");

  wfp = fopen(dump_file_name, WRITE_MODE);
  if (!wfp)
    {
      fprintf(stderr, "error: cannot open \"%s\"\n", dump_file_name);
      exit(EXIT_FAILURE);
    }

  fprintf(wfp, control_string, 0 /*val_stk_size */ );
  fprintf(wfp, control_string, 0 /*cxt_stk_size */ );
  fprintf(wfp, control_string, contigify(e_boot_code));
  fprintf(wfp, control_string,
	  free_point - new_space.start + (just_new ? 0 : spatic.size));

  /* Maybe dump spatic space. */
  if (!just_new)
    for (memptr = spatic.start; memptr < spatic.end; memptr++)
      {
	if (eighter == 0)
	  fprintf(wfp, "\n");
	theref = *memptr;
	CONTIGIFY(theref);
	fprintf(wfp, control_string, theref);
	eighter = (eighter + 1) % 8;
      }
  eighter = 0;
  for (memptr = new_space.start; memptr < free_point; memptr++)
    {
      if (eighter == 0)
	fprintf(wfp, "\n");
      theref = *memptr;
      CONTIGIFY(theref);
      fprintf(wfp, control_string, theref);
      eighter = (eighter + 1) % 8;
    }
  fprintf(wfp, "\n");

  /* Write the weak pointer table. */

  fprintf(wfp, control_string, wp_index);

  eighter = 0;

  for (i = 0; i < wp_index; i++)
    {
      if (eighter == 0)
	fprintf(wfp, "\n");
      theref = wp_table[1 + i];
      CONTIGIFY(theref);
      fprintf(wfp, control_string, theref);
      eighter = (eighter + 1) % 8;
    }

  fclose(wfp);
}

void
dump_world(bool just_new)
{
  fprintf(stderr, "About to dump the oaklisp world.\n");
  if (dump_base == 2)
    dump_binary_world(just_new);
  else
    dump_ascii_world(just_new);
}

static void
reoffset(ref_t baseAddr,
	 ref_t * start,
	 long count)
{
  long index;
  ref_t *next;

  next = start;
  for (index = 0; index < count; index++)
    {
      if (*next & 2)
	*next += baseAddr;
      next++;
    }
}

void
read_world(char *str)
{
  FILE *d;
  int magichar;


  if ((d = fopen(str, READ_BINARY_MODE)) == 0)
    {
      printf("Can't open \"%s\".\n", str);
      exit(EXIT_FAILURE);
    }
  magichar = getc(d);
  if (magichar == (int)'\002')
    {
      getc(d);
      getc(d);
      getc(d);
      input_is_binary = 1;
    }
  else
    {
      ungetc(magichar, d);
      input_is_binary = 0;
#ifdef WORDS_BIGENDIAN
      printf("Big Endian.\n");
#else
      printf("Little Endian.\n");
#endif
    }

  /* Obsolescent: read val_space_size and cxt_space_size: */
  (void)read_ref(d);
  (void)read_ref(d);

  e_boot_code = read_ref(d);

  spatic.size = (size_t) read_ref(d);
  alloc_space(&spatic, spatic.size);

  e_boot_code += (ref_t) spatic.start;

  {
    long load_count;
    ref_t *mptr, next;

    load_count = spatic.size;
    mptr = spatic.start;

    if (input_is_binary)
      {
	xfread((void *)spatic.start, sizeof(ref_t), load_count, d);
	reoffset((ref_t) spatic.start, spatic.start, load_count);
      }
    else
      while (load_count != 0)
	{
	  next = read_ref(d);
	  if (next & 2)
	    next += (ref_t) spatic.start;
	  *mptr++ = next;
	  --load_count;
	}

    /* Load the weak pointer table. */
    wp_index = read_ref(d);

    if (wp_index + 1 > wp_table_size)
      {
	fprintf(stderr,
		"Error (loading world): number of weak pointers in world"
		" exceeds internal table size.\n");
	exit(EXIT_FAILURE);
      }

    load_count = wp_index;
    mptr = &wp_table[1];

    if (input_is_binary)
      {
	xfread((void *)&wp_table[1], sizeof(ref_t), (long)wp_index, d);
	reoffset((ref_t) spatic.start, &wp_table[1], wp_index);
      }
    else
      while (load_count != 0)
	{
	  next = read_ref(d);
	  if (next & 2)
	    next += (ref_t) spatic.start;
	  *mptr++ = next;
	  --load_count;
	}
  }

  /* The weak pointer hash table is rebuilt when e_nil is set. */
  fclose(d);
}
