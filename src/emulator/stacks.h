/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#ifndef _STACKS_H_INCLUDED
#define _STACKS_H_INCLUDED

#include "config.h"
#include "gc.h"
#include "data.h"

extern int max_segment_size;


/* flushed stack segment.  Allocated and gc'ed in the oaklisp heap. */
typedef struct {
  /* Do not rearange this structure or you'll be sorry! */
  ref_t type_field;
  ref_t length_field;
  ref_t previous_segment;
  ref_t data[1];
} segment_t;

#define SEGMENT_HEADER_LENGTH (sizeof(segment_t)/sizeof(ref_t)-1)

/* stack type */

typedef struct {
  int size;			/* size of stack buffer */
  int filltarget;		/* how high to fill buffer ideally */
  ref_t *bp;			/* pointer to this stack's "buffer" */
  ref_t *sp;			/* pointer to top element in stack */
  ref_t segment;		/* head of linked list of flushed segments */
  int pushed_count;		/* number of ref's in flushed segment list */
} stack_t;

#ifdef THREADS
extern stack_t *value_stack_array[];
extern stack_t *cntxt_stack_array[];
#else
extern stack_t value_stack;
extern stack_t context_stack;
#endif

extern void init_stacks(void);
extern void stack_flush(stack_t * stack_p, int amount_to_leave);
extern void stack_unflush(stack_t * stack_p, int n);
extern void dump_stack(stack_t * stack_p);

#endif
