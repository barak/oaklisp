/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "data.h"
#include "xmalloc.h"
#include "gc.h"
#include "stacks.h"

int max_segment_size = 256;

void
stack_flush(stack_t * stack_p, int amount_to_leave)
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


/* This routine grabs some segments that have been flushed from the buffer
   and puts them back in.  Because the segments might be small, it
   may have to put more than one segment back in.  It grabs enough so that
   the buffer has at least n+1 values in it, so that at least n values could
   be popped off without underflow. */

void
stack_unflush(stack_t * stack_p, int n)
{
  long i, number_to_pull = 0;
  long count = stack_p->sp - stack_p->bp + 1;
  long new_count = count;
  segment_t *s = (segment_t *) REF_TO_PTR(stack_p->segment);
  ref_t *dest;

#ifndef FAST
  if (trace_segs) printf("seg:unflush-");
#endif

  /* First, figure out how many segments to pull. */
  for (; new_count <= n; s = (segment_t *) REF_TO_PTR(s->previous_segment))
    {
      int this_one = REF_TO_INT(s->length_field) - SEGMENT_HEADER_LENGTH;

#ifndef FAST
      if (trace_segs) printf("%d-", this_one);
#endif

      new_count += this_one;
      number_to_pull += 1;
    }

#ifndef FAST
  if (trace_segs) printf("(%ld)-", number_to_pull);
#endif

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


void
dump_stack(stack_t * stack_p)
{
  /* dump part of stack, which is not segmented */
  ref_t *p;
  fprintf(stdout, "stack contents (height: %d): ",
	  stack_p->sp - stack_p->bp + 1 + stack_p->pushed_count);

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
  my_index_p = pthread_getspecific (index_key);
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
  value_stack.segment = INT_TO_REF(0);
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
  context_stack.segment = INT_TO_REF(0);
  context_stack.pushed_count = 0;
}


