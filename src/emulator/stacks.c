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
#include "stacks.h"
#include "threads.h"

int max_segment_size = 256;


/* Called when a stack has to be unflushed but the flushed segment list
   is empty, i.e. when more values are needed than the stack ever held.
   Does not return. */

static void
stack_underflow(oakstack * stack_p, int n)
{
  const char *which;

#ifdef THREADS
  int *my_index_p = (int *)oak_tls_get(index_key);
  int my_index = (my_index_p == NULL) ? 0 : *my_index_p;

  /* A spawned thread starts with an empty context stack, so the RETURN
     that leaves its thunk's outermost frame has no context to pop.
     That is a thunk running to completion rather than a VM error: leave
     the interpreter and let the thread exit. */
  if (stack_p == cntxt_stack_array[my_index] && oak_thread_can_exit())
    oak_thread_exit_unwind();	/* does not return */

  which = (stack_p == cntxt_stack_array[my_index]) ? "context" : "value";
  fprintf(stderr,
	  "Fatal error (vm): %s stack underflow in thread %d: %d value%s "
	  "wanted, none flushed.\n",
	  which, my_index, n, (n == 1) ? "" : "s");
#else
  which = (stack_p == &context_stack) ? "context" : "value";
  fprintf(stderr,
	  "Fatal error (vm): %s stack underflow: %d value%s wanted, "
	  "none flushed.\n",
	  which, n, (n == 1) ? "" : "s");
#endif

  fflush(stderr);
  exit(EXIT_FAILURE);
}


void
stack_flush(oakstack * stack_p, int amount_to_leave)
{
  /* flushes out the value stack buffer, leaving amount_to_leave */
  segment_t *s;
  int i;
  int count = stack_p->sp - stack_p->bp + 1;
  int amount_to_flush = count - amount_to_leave;
  int amount_unflushed = amount_to_flush;
  ref_t *src = stack_p->bp;
  ref_t *end = stack_p->sp - amount_to_leave;

  /* flush everything between src & end, them move portion of buffer
     after end down to beginning of buffer. */

#ifndef FAST
  if (trace_segs) printf("seg:flush-");
#endif

  while (src <= end)
    {
      /* Flush a single segment. */
      long size = amount_unflushed;

      if (size > max_segment_size)
	size = max_segment_size;

      /* allocate a new segment */
      {
	ref_t *p;
	ALLOCATE(p, (size + SEGMENT_HEADER_LENGTH),
		 "space crunch allocating stack segment");
	s = (segment_t *)p;
      }

      /* fill in header of new segment */
      s->type_field = e_segment_type;
      s->length_field = INT_TO_REF(size + SEGMENT_HEADER_LENGTH);

      /* link segment onto head of flushed segment list */
      s->previous_segment = stack_p->segment;
      stack_p->segment = PTR_TO_REF(s);

      for (i = 0; i < size; i++)
	s->data[i] = *src++;

      amount_unflushed -= size;

#ifndef FAST
      if (trace_segs) printf("%ld-", size);
#endif
    }

  for (i = 0; i < amount_to_leave; i++)
    stack_p->bp[i] = *src++;

  stack_p->sp = &stack_p->bp[amount_to_leave - 1];
  stack_p->pushed_count += amount_to_flush;

#ifndef FAST
  if (trace_segs) printf(".\n");
#endif
}


/* Called when more values are asked for than the buffer can hold.  The
   caller has to pop through the segments in more than one pass instead;
   see stack_pop_n.  Does not return. */

static void
stack_overunflush(oakstack * stack_p, long wanted)
{
  fprintf(stderr,
	  "Fatal error (vm): unflush of %ld values into a %d value stack "
	  "buffer.\n", wanted, stack_p->size);
  fflush(stderr);
  exit(EXIT_FAILURE);
}


/* This routine grabs some segments that have been flushed from the buffer
   and puts them back in.  Because the segments might be small, it
   may have to put more than one segment back in.  It grabs enough so that
   the buffer has at least n+1 values in it, so that at least n values could
   be popped off without underflow.

   The caller must not ask for more values than the buffer holds: the
   command line code guarantees room for a maximal segment on top of the
   deepest access any instruction makes, and anything that has to reach
   further down than that goes through stack_pop_n. */

void
stack_unflush(oakstack * stack_p, int n)
{
  long i, number_to_pull = 0;
  long count = stack_p->sp - stack_p->bp + 1;
  long new_count = count;
  ref_t seg = stack_p->segment;
  segment_t *s;
  ref_t *dest;

#ifndef FAST
  if (trace_segs) printf("seg:unflush-");
#endif

  /* First, figure out how many segments to pull. */
  for (; new_count <= n; seg = s->previous_segment)
    {
      int this_one;

      /* Nothing left to pull in.  Walking off the end of the list would
	 read nil's second slot as a segment length and copy a wild
	 amount of data, so report the underflow instead. */
      if (seg == e_nil)
	stack_underflow(stack_p, n);

      s = (segment_t *) REF_TO_PTR(seg);
      this_one = REF_TO_INT(s->length_field) - SEGMENT_HEADER_LENGTH;

#ifndef FAST
      if (trace_segs) printf("%d-", this_one);
#endif

      new_count += this_one;
      number_to_pull += 1;
    }

#ifndef FAST
  if (trace_segs) printf("(%ld)-", number_to_pull);
#endif

  /* The shuffle below and the segment copy after it both write as far
     as bp[new_count-1], so refuse to start rather than run off the end
     of the buffer. */
  if (new_count > stack_p->size)
    stack_overunflush(stack_p, new_count);

  /* Copy the data in the buffer up to its new home. */
  dest = &stack_p->bp[new_count - 1];

  for (i = count - 1; i >= 0; i--)
    *dest-- = stack_p->bp[i];

  /* Suck in the segments. */
  for (s = (segment_t *) REF_TO_PTR(stack_p->segment);
       number_to_pull > 0; number_to_pull--)
    {
      /* Suck in this segment. */
      for (i = REF_TO_INT(s->length_field) - SEGMENT_HEADER_LENGTH - 1
	   ; i >= 0; i--)
	*dest-- = s->data[i];
      s = (segment_t *) REF_TO_PTR(s->previous_segment);

#ifndef FAST
      if (trace_segs) printf("p");
#endif
    }

  stack_p->segment = PTR_TO_REF(s);
  stack_p->sp = &stack_p->bp[new_count - 1];
  stack_p->pushed_count -= (int)(new_count - count);

#ifndef FAST
  if (trace_segs)
    printf(".\n");
#endif
}


/* Pop n values off a stack.  Unlike unflushing and then moving the
   pointer, this does not require the values to be in the buffer all at
   once: whole flushed segments that fall entirely within the pop are
   dropped rather than copied back in, and only the segment the boundary
   lands in is unflushed.  A deep unwind -- a THROW or an error escape
   out of a recursion thousands of frames deep -- pops far more values
   than the buffer holds, and asking stack_unflush for them wrote past
   the end of it. */

void
stack_pop_n(oakstack * stack_p, long n)
{
  long count = stack_p->sp - stack_p->bp + 1;

  if (n <= 0)
    return;

  /* Everything to pop is in the buffer, and popping it leaves the top
     of the stack visible. */
  if (n < count)
    {
      stack_p->sp -= n;
      return;
    }

  /* Otherwise throw the whole buffer away and work through the
     segments. */
  n -= count;
  stack_p->sp = stack_p->bp - 1;

  while (n > 0)
    {
      segment_t *s;
      long this_one;

      if (stack_p->segment == e_nil)
	stack_underflow(stack_p, (int)n);

      s = (segment_t *) REF_TO_PTR(stack_p->segment);
      this_one = REF_TO_INT(s->length_field) - SEGMENT_HEADER_LENGTH;

      /* The boundary falls inside this segment; leave it to the
	 unflush below. */
      if (this_one > n)
	break;

      stack_p->segment = s->previous_segment;
      stack_p->pushed_count -= (int)this_one;
      n -= this_one;

#ifndef FAST
      if (trace_segs) printf("seg:drop-%ld.\n", this_one);
#endif
    }

  /* Pull one segment back in so the top of the stack is visible again
     and the remainder can be popped from the buffer.  A segment never
     exceeds max_segment_size, which the buffer is sized to hold.  With
     nothing left to pull in there is nothing under the pop either,
     which is the underflow the ordinary pop path reports. */
  if (n > 0 || stack_p->segment != e_nil)
    {
      stack_unflush(stack_p, (int)n);
      stack_p->sp -= n;
    }
  else
    stack_underflow(stack_p, 1);
}


void
dump_stack(oakstack * stack_p)
{
  /* dump part of stack, which is not segmented */
  ref_t *p;
  fprintf(stdout, "stack contents (height: %lu): ",
	  (unsigned long)(stack_p->sp - stack_p->bp + 1 + stack_p->pushed_count));

  for (p = stack_p->bp; p <= stack_p->sp; ++p)
    {
      printref(stdout, *p);
      putc(p == stack_p->sp ? '\n' : ' ', stdout);
    }
  fflush(stdout);
}

void
init_stacks(void)
{
#ifdef THREADS
  int *my_index_p;
  int my_index;
#endif

  ref_t *ptr;

  /* For debugging we allocate two ref_t more
     and initialise these with a special pattern
     to detect out-of-range writes with assert()
   */

  /* Initialise value stack */
#ifdef THREADS
  my_index_p = oak_tls_get(index_key);
  my_index = *my_index_p;
#endif

  ptr = (ref_t *) xmalloc((value_stack.size + 2)
			  * sizeof(ref_t));
  *ptr = PATTERN;
  ptr[value_stack.size + 1] = PATTERN;
  value_stack.bp = ptr + 1;
  value_stack.sp = value_stack.bp;
  *value_stack.bp = INT_TO_REF(1234);

  /* This becomes e_nil when segment_type is loaded. */
  value_stack.segment = e_nil;
  value_stack.pushed_count = 0;

  /* Initialise context stack */


  ptr = (ref_t *) xmalloc((context_stack.size + 2)
			  * sizeof(ref_t));
  *ptr = PATTERN;
  ptr[context_stack.size + 1] = PATTERN;
  context_stack.bp = ptr + 1;
  context_stack.sp = context_stack.bp;
  *context_stack.bp = INT_TO_REF(1234);

  /* This becomes e_nil when segment_type is loaded. */
  context_stack.segment = e_nil;
  context_stack.pushed_count = 0;
}
