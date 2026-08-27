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
#include <limits.h>
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


static bool input_is_binary;

/* First token of an ascii world dumped by this emulator; see
   dump_ascii_world and read_world. */
#ifdef WORDS_BIGENDIAN
#define ASCII_WORLD_ENDIAN "be"
#else
#define ASCII_WORLD_ENDIAN "le"
#endif
#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
#define ASCII_WORLD_TAG \
  "oak" STRINGIFY(__WORDSIZE) ASCII_WORLD_ENDIAN


/* These are for making the world zero-based and contiguous in dumps. */

static ref_t
contig(ref_t r, bool just_new)
{
  ref_t *p = ANY_TO_PTR(r);

  /* A reference that points outside both spaces cannot be made
     relative, so the dump would be written with a wild pointer in it
     and the damage only found at the next boot.  The other failures in
     this file exit; so does this one. */

  if (just_new)
    {
      if (NEW_PTR(p))
	return ((ref_t) (p - new_space.start) << REF_SHIFT) | (r & TAG_MASK);
      fprintf(stderr,
	      "Error (dumping world): non-new pointer %zu found.\n",
	      (size_t)r);
    }
  else if (SPATIC_PTR(p))
    return ((ref_t) (p - spatic.start) << REF_SHIFT) | (r & TAG_MASK);
  else if (NEW_PTR(p))
    return ((ref_t) (p - new_space.start + spatic.size) << REF_SHIFT) | (r & TAG_MASK);
  else
    fprintf(stderr,
	    "Error (dumping world): non-new or spatic pointer %zu found.\n",
	    (size_t)r);
  fflush(stderr);
  exit(EXIT_FAILURE);
}

#define contigify(r) ((r)&PTR_MASK ? contig((r),just_new) : (r))
#define CONTIGIFY(v) { if ((v)&PTR_MASK) (v) = contig((v),just_new); }


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
      unsigned long long b;
      fscanf(d, " ");
      bool swapem = (c = getc(d)) == '^';
      if (!swapem) ungetc(c, d);
      if (fscanf(d, "%llx", &b) != 1)
	{
	  printf("Error reading cold load file, might be truncated.\n");
	  exit(EXIT_FAILURE);
	}
      a = (ref_t)b;
#ifndef WORDS_BIGENDIAN
      if (swapem)
	a = ((a&0xFFFF) << 16 | (a&0xFFFF0000) >> 16);
#endif
      return a;
    }				/* input_is_binary */
}


#define REFBUFSIZ 256

static ref_t refbuf[REFBUFSIZ];

/* Close the world file, reporting anything that went wrong on the way
   out.  Unchecked writes turn a full disk into a silently truncated
   world image, which only fails at the next boot. */

static void
finish_world_file(FILE *wfp)
{
  int bad = ferror(wfp);

  if (fclose(wfp) != 0)
    bad = 1;
  if (bad)
    {
      fprintf(stderr, "error: writing \"%s\" failed;"
	      " the world image is incomplete.\n", dump_file_name);
      exit(EXIT_FAILURE);
    }
}

static void
dump_binary_world(bool just_new)
{
  FILE *wfp = 0;
  ref_t *memptr;
  ref_t theref;

  /* CAUTION: STACK SPACE!!! */

  int imod = 0;
  ref_t worlsiz = free_point - new_space.start;
  ref_t DUMMY = 0;

  fprintf(stderr, "Dumping in binary.\n");

  wfp = fopen(dump_file_name, WRITE_BINARY_MODE);
  if (!wfp)
    {
      fprintf(stderr, "error opening \"%s\"\n", dump_file_name);
      exit(EXIT_FAILURE);
    }

  if (!just_new)
    worlsiz += spatic.size;

  /* Magic bytes: \002\002\002\002 for 32-bit, \004\004\004\004 for 64-bit */
#if __WORDSIZE == 64
  putc('\004', wfp);
  putc('\004', wfp);
  putc('\004', wfp);
  putc('\004', wfp);
#else
  putc('\002', wfp);
  putc('\002', wfp);
  putc('\002', wfp);
  putc('\002', wfp);
#endif

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

  finish_world_file(wfp);
}


static void
dump_ascii_world(bool just_new)
{
  ref_t *memptr, theref;
  long i;
  int eighter = 0;
  /* Always hexadecimal: read_ref() parses ascii worlds with "%llx",
     so a decimal dump could never be read back. */
  char *control_string = "%zx ";
  FILE *wfp = 0;

  fprintf(stderr, "Dumping in ascii.\n");

  wfp = fopen(dump_file_name, WRITE_MODE);
  if (!wfp)
    {
      fprintf(stderr, "error: cannot open \"%s\"\n", dump_file_name);
      exit(EXIT_FAILURE);
    }

  /* Word size and byte order marker.  A binary world carries one in
     its magic bytes, and read_world refuses a world that does not
     match; an ascii world had none, so a 32-bit one loaded into a
     64-bit emulator with every tagged value shifted by the wrong
     amount and no complaint.  Cold worlds written by oak-cold-linker
     still have no marker, and read_world still accepts that. */
  fprintf(wfp, "%s\n", ASCII_WORLD_TAG);

  fprintf(wfp, control_string, (size_t)0 /*val_stk_size */ );
  fprintf(wfp, control_string, (size_t)0 /*cxt_stk_size */ );
  fprintf(wfp, control_string, (size_t)contigify(e_boot_code));
  fprintf(wfp, control_string,
	  (size_t)(free_point - new_space.start + (just_new ? 0 : spatic.size)));

  /* Maybe dump spatic space. */
  if (!just_new)
    for (memptr = spatic.start; memptr < spatic.end; memptr++)
      {
	if (eighter == 0)
	  fprintf(wfp, "\n");
	theref = *memptr;
	CONTIGIFY(theref);
	fprintf(wfp, control_string, (size_t)theref);
	eighter = (eighter + 1) % 8;
      }
  eighter = 0;
  for (memptr = new_space.start; memptr < free_point; memptr++)
    {
      if (eighter == 0)
	fprintf(wfp, "\n");
      theref = *memptr;
      CONTIGIFY(theref);
      fprintf(wfp, control_string, (size_t)theref);
      eighter = (eighter + 1) % 8;
    }
  fprintf(wfp, "\n");

  /* Write the weak pointer table. */

  fprintf(wfp, control_string, (size_t)wp_index);

  eighter = 0;

  for (i = 0; i < wp_index; i++)
    {
      if (eighter == 0)
	fprintf(wfp, "\n");
      theref = wp_table[1 + i];
      CONTIGIFY(theref);
      fprintf(wfp, control_string, (size_t)theref);
      eighter = (eighter + 1) % 8;
    }

  finish_world_file(wfp);
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
  if (magichar == (int)'\002' || magichar == (int)'\004')
    {
      /* Verify world matches our pointer size */
#if __WORDSIZE == 64
      if (magichar != (int)'\004')
	{
	  printf("Error: 32-bit world loaded into 64-bit emulator.\n");
	  exit(EXIT_FAILURE);
	}
#else
      if (magichar != (int)'\002')
	{
	  printf("Error: 64-bit world loaded into 32-bit emulator.\n");
	  exit(EXIT_FAILURE);
	}
#endif
      getc(d);
      getc(d);
      getc(d);
      input_is_binary = 1;
    }
  else
    {
      ungetc(magichar, d);
      input_is_binary = 0;

      /* An ascii world dumped by this emulator starts with a word size
	 and byte order marker.  A cold world written by oak-cold-linker
	 has none, so its absence is not an error, but a marker that
	 disagrees with this emulator is. */
      if (magichar == (int)'o')
	{
	  char tag[16];

	  if (fscanf(d, "%15s", tag) != 1)
	    {
	      printf("Error: cannot read world file header of \"%s\".\n", str);
	      exit(EXIT_FAILURE);
	    }
	  if (strcmp(tag, ASCII_WORLD_TAG) != 0)
	    {
	      printf("Error: ascii world \"%s\" is %s, this emulator is %s.\n",
		     str, tag, ASCII_WORLD_TAG);
	      exit(EXIT_FAILURE);
	    }
	}

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

  /* The word count comes out of the file and goes straight into
     xmalloc(sizeof(ref_t) * count), so a count near SIZE_MAX/8 would
     wrap to a small allocation that the read below then overruns.  The
     weak pointer count further down is range checked the same way. */
  {
    ref_t world_size = read_ref(d);

    if ((ssize_t)world_size < 0
	|| (size_t)world_size > SIZE_MAX / sizeof(ref_t))
      {
	fprintf(stderr,
		"Error (loading world): bogus world size %zu.\n",
		(size_t)world_size);
	exit(EXIT_FAILURE);
      }
    spatic.size = (size_t) world_size;
  }
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

    /* Load the weak pointer table.  The count comes out of the world
       file, so it has to be checked before it is narrowed to the int
       wp_index is: a truncated or corrupt world naming a count above
       INT_MAX would wrap to a negative and slip past the test. */
    {
      ref_t wp_count = read_ref(d);

      if ((ssize_t)wp_count < 0 || wp_count >= (ref_t)INT_MAX)
	{
	  fprintf(stderr,
		  "Error (loading world): bogus weak pointer count %zu.\n",
		  (size_t)wp_count);
	  exit(EXIT_FAILURE);
	}
      wp_index = (int)wp_count;
    }

    /* The tables grow on demand, so a world with more weak pointers
       than the current capacity is fine; just make room for them. */
    ensure_wp_capacity(wp_index + 1);

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
