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
#include "oak-header.h"


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
 * OPTIONAL: ;oaklisp-world format=... word-size=... instructions-per-ref=... [endian=...]
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
 *
 * The header line (see oak-header.h) identifies the architecture the
 * world is for.  Its format field is "binary" for raw refs in native
 * byte order, "cold" for the hex text produced by the cold linkers
 * (in which cells holding a pair of opcodes are marked with a ^ and
 * are byte-order independent), or "hex"/"decimal" for text dumps of
 * raw refs.  Worlds without a header are legacy: four bytes of \002
 * (32-bit) or \004 (64-bit) introduce a binary world, anything else
 * is a cold world.
 */


static bool input_is_binary;
static bool input_is_decimal;

static const char *
world_header(const char *format)
{
  static char buf[OAK_HEADER_MAX];
  snprintf(buf, sizeof(buf),
	   ";oaklisp-world format=%s endian=%s word-size=%d instructions-per-ref=%d\n",
	   format, OAK_ENDIAN_NAME, OAK_WORD_SIZE, INSTRS_PER_REF);
  return buf;
}

/* Check a world header against this emulator, exiting on mismatch. */
static void
check_world_header(const char *file, oak_header_t *h)
{
  if (strcmp(h->kind, "world") != 0)
    {
      fprintf(stderr, "error: \"%s\" is an oaklisp-%s file, not a world.\n",
	      file, h->kind);
      exit(EXIT_FAILURE);
    }
  if (h->word_size != 0 && h->word_size != OAK_WORD_SIZE)
    {
      fprintf(stderr,
	      "error: world \"%s\" is for %d-bit refs but this emulator uses %d-bit refs.\n",
	      file, h->word_size, OAK_WORD_SIZE);
      exit(EXIT_FAILURE);
    }
  if (h->instrs_per_ref != 0 && h->instrs_per_ref != INSTRS_PER_REF)
    {
      fprintf(stderr,
	      "error: world \"%s\" has %d instructions per ref but this emulator uses %d.\n",
	      file, h->instrs_per_ref, INSTRS_PER_REF);
      exit(EXIT_FAILURE);
    }
  if (h->endian[0] != '\0' && strcmp(h->endian, OAK_ENDIAN_NAME) != 0)
    {
      fprintf(stderr,
	      "error: world \"%s\" is %s-endian but this emulator is %s-endian.\n",
	      file, h->endian, OAK_ENDIAN_NAME);
      exit(EXIT_FAILURE);
    }
}


/* These are for making the world zero-based and contiguous in dumps. */

static ref_t
contig(ref_t r, bool just_new)
{
  ref_t *p = ANY_TO_PTR(r);

  if (just_new)
    if (NEW_PTR(p))
      return ((ref_t) (p - new_space.start) << REF_SHIFT) | (r & TAG_MASK);
    else
      printf("Non-new pointer %zu found.\n", (size_t)r);
  else if (SPATIC_PTR(p))
    return ((ref_t) (p - spatic.start) << REF_SHIFT) | (r & TAG_MASK);
  else if (NEW_PTR(p))
    return ((ref_t) (p - new_space.start + spatic.size) << REF_SHIFT) | (r & TAG_MASK);
  else
    printf("Non-new or spatic pointer %zu found.\n", (size_t)r);
  return r;
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
      if (fscanf(d, input_is_decimal ? "%llu" : "%llx", &b) != 1)
	{
	  printf("Error reading cold load file, might be truncated.\n");
	  exit(EXIT_FAILURE);
	}
      a = (ref_t)b;
      /* A ^-marked cell holds two opcodes, written as the first
	 opcode in the high 16 bits and the second in the low 16 bits
	 of a 32-bit quantity.  Arrange them so that the first opcode
	 comes first in memory. */
      if (swapem)
	{
#ifdef WORDS_BIGENDIAN
	  a <<= (OAK_WORD_SIZE - 32);
#else
	  a = ((a&0xFFFF) << 16 | (a&0xFFFF0000) >> 16);
#endif
	}
      return a;
    }				/* input_is_binary */
}


#define REFBUFSIZ 256

static ref_t refbuf[REFBUFSIZ];

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

  fputs(world_header("binary"), wfp);

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
  char *control_string = (dump_base == 10 ? "%zd " : "%zx ");
  FILE *wfp = 0;

  fprintf(stderr, "Dumping in ascii.\n");

  wfp = fopen(dump_file_name, WRITE_MODE);
  if (!wfp)
    {
      fprintf(stderr, "error: cannot open \"%s\"\n", dump_file_name);
      exit(EXIT_FAILURE);
    }

  fputs(world_header(dump_base == 10 ? "decimal" : "hex"), wfp);

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
  input_is_binary = 0;
  input_is_decimal = 0;
  if (magichar == (int)';')
    {
      oak_header_t h;
      if (!oak_read_header(d, &h))
	{
	  printf("Error: \"%s\" does not start with an Oaklisp header.\n", str);
	  exit(EXIT_FAILURE);
	}
      check_world_header(str, &h);
      if (!strcmp(h.format, "binary"))
	input_is_binary = 1;
      else if (!strcmp(h.format, "decimal"))
	input_is_decimal = 1;
      else if (strcmp(h.format, "cold") && strcmp(h.format, "hex"))
	{
	  printf("Error: world \"%s\" has unknown format \"%s\".\n", str, h.format);
	  exit(EXIT_FAILURE);
	}
    }
  else if (magichar == (int)'\002' || magichar == (int)'\004')
    {
      /* Legacy binary world; the magic byte encodes the ref size. */
      int world_bits = (magichar == (int)'\004') ? 64 : 32;
      if (world_bits != OAK_WORD_SIZE)
	{
	  printf("Error: %d-bit world loaded into %d-bit emulator.\n",
		 world_bits, OAK_WORD_SIZE);
	  exit(EXIT_FAILURE);
	}
      getc(d);
      getc(d);
      getc(d);
      input_is_binary = 1;
    }
  else
    {
      /* Legacy cold world, no header. */
      ungetc(magichar, d);
      printf("%s-endian %d-bit.\n",
	     OAK_ENDIAN_NAME[0] == 'b' ? "Big" : "Little", OAK_WORD_SIZE);
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
