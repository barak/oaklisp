// This file is part of Oaklisp.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// The GNU GPL is available at http://www.gnu.org/licenses/gpl.html
// or from the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA


/*
 * Architecture header lines for Oaklisp files.
 *
 * Compiled bytecode (.oa), cold worlds (.cold) and dumped worlds
 * (.bin) may begin with a single header line identifying the
 * architecture they were built for:
 *
 *   ;oaklisp-bytecode word-size=32 instructions-per-ref=2
 *   ;oaklisp-world format=cold word-size=64 instructions-per-ref=2
 *   ;oaklisp-world format=binary endian=little word-size=64 instructions-per-ref=2
 *
 * The line starts with ";oaklisp-" (so the Oaklisp reader treats it
 * as a comment) followed by whitespace-separated key=value pairs.
 * Unknown keys are ignored.  Files lacking a header are legacy files,
 * which are assumed to be for the "bc2" (two instructions per ref)
 * bytecode; their word size is unknown.
 *
 * Architecture names, used for prebuilt/ subdirectories and the
 * --target options, are "bc<instructions-per-ref>-<word-size>" for
 * bytecode (e.g. bc2-64) and "bc<instructions-per-ref>-<el|eb><word-size>"
 * for worlds (e.g. bc2-el64).
 *
 * This is a header-only implementation so that oak-cold-linker.c
 * remains buildable as a single translation unit.
 */

#ifndef _OAK_HEADER_H_INCLUDED
#define _OAK_HEADER_H_INCLUDED

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define OAK_HEADER_MAX 512

typedef struct {
  char kind[32];		/* "world" or "bytecode" */
  char format[16];		/* world: cold, binary, hex, decimal; else "" */
  char endian[8];		/* "little", "big", or "" if unspecified */
  int word_size;		/* 32 or 64, 0 if unspecified */
  int instrs_per_ref;		/* 2 (or 4), 0 if unspecified */
} oak_header_t;

/* Copy at most n-1 chars of src into dst, NUL terminated. */
static inline void
oak_header_copy(char *dst, size_t n, const char *src, size_t len)
{
  if (len >= n) len = n - 1;
  memcpy(dst, src, len);
  dst[len] = '\0';
}

/* Parse a header line.  LINE may or may not include the leading ';'
   and trailing newline.  Returns 1 if the line is an Oaklisp header,
   0 otherwise.  Fields absent from the line are left zero/empty. */
static inline int
oak_parse_header(const char *line, oak_header_t *h)
{
  const char *p = line;
  memset(h, 0, sizeof(*h));
  while (*p == ';') p++;
  if (strncmp(p, "oaklisp-", 8) != 0)
    return 0;
  p += 8;
  {
    const char *e = p;
    while (*e && !isspace((unsigned char)*e)) e++;
    oak_header_copy(h->kind, sizeof(h->kind), p, e - p);
    p = e;
  }
  for (;;)
    {
      const char *key, *eq, *end;
      while (*p && isspace((unsigned char)*p)) p++;
      if (!*p) break;
      key = p;
      while (*p && !isspace((unsigned char)*p)) p++;
      end = p;
      eq = memchr(key, '=', end - key);
      if (!eq) continue;		/* bare token; ignore */
      {
	size_t klen = eq - key;
	const char *val = eq + 1;
	size_t vlen = end - val;
	if (klen == 6 && !strncmp(key, "format", 6))
	  oak_header_copy(h->format, sizeof(h->format), val, vlen);
	else if (klen == 6 && !strncmp(key, "endian", 6))
	  oak_header_copy(h->endian, sizeof(h->endian), val, vlen);
	else if (klen == 9 && !strncmp(key, "word-size", 9))
	  h->word_size = atoi(val);
	else if (klen == 20 && !strncmp(key, "instructions-per-ref", 20))
	  h->instrs_per_ref = atoi(val);
	/* else: unknown key, ignored for forward compatibility */
      }
    }
  return 1;
}

/* Read a header line from FP, whose first character (a ';') has
   already been consumed.  Returns 1 if it was an Oaklisp header. */
static inline int
oak_read_header(FILE *fp, oak_header_t *h)
{
  char buf[OAK_HEADER_MAX];
  int c;
  size_t i = 0;
  while ((c = getc(fp)) != EOF && c != '\n')
    if (i < sizeof(buf) - 1)
      buf[i++] = (char)c;
  buf[i] = '\0';
  return oak_parse_header(buf, h);
}

/* Parse an architecture name such as "bc2-64" or "bc2-el64" into H.
   Returns 1 on success, 0 if the name is malformed. */
static inline int
oak_parse_arch_name(const char *name, oak_header_t *h)
{
  const char *p = name;
  char *end;
  memset(h, 0, sizeof(*h));
  if (strncmp(p, "bc", 2) != 0)
    return 0;
  p += 2;
  h->instrs_per_ref = (int)strtol(p, &end, 10);
  if (end == p || *end != '-')
    return 0;
  p = end + 1;
  if (!strncmp(p, "el", 2))
    { strcpy(h->endian, "little"); p += 2; }
  else if (!strncmp(p, "eb", 2))
    { strcpy(h->endian, "big"); p += 2; }
  h->word_size = (int)strtol(p, &end, 10);
  if (end == p || *end != '\0')
    return 0;
  if (h->word_size != 32 && h->word_size != 64)
    return 0;
  if (h->instrs_per_ref <= 0)
    return 0;
  return 1;
}

#endif /* _OAK_HEADER_H_INCLUDED */
