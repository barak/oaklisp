/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "data.h"
#include "weak.h"
#include "xmalloc.h"
#include "stacks.h"
#include "gc.h"
#include "threads.h"


#ifdef USE_VADVISE
#include <sys/vadvise.h>
#endif



/* 1/RECLAIM_FACTOR is the target for how much of new space should 
   be used after a gc.  If more than this is used, the next new 
   space allocated will be bigger. */

#define RECLAIM_FACTOR 3

bool full_gc = false;

ref_t pre_gc_nil;

unsigned long transport_count;
unsigned long loc_transport_count;

#ifndef GC_EXAMINE_BUFFER_SIZE
#define GC_EXAMINE_BUFFER_SIZE 16
#endif

ref_t gc_examine_buffer[GC_EXAMINE_BUFFER_SIZE];
ref_t *gc_examine_ptr = gc_examine_buffer;

#define GC_TOUCH(x)			\
{					\
  if ((x)&PTR_MASK)			\
    {					\
      ref_t *MACROp = ANY_TO_PTR((x));	\
					\
      if (OLD_PTR(MACROp))		\
	(x) = gc_touch0((x));		\
    }					\
}

#define GC_TOUCH_PTR(r,o)					\
{								\
  (r) = REF_TO_PTR(gc_touch0(PTR_TO_REF((r)-(o)))) + (o);	\
}



#define LOC_TOUCH(x)				\
{						\
  if (TAG_IS((x),LOC_TAG))			\
    {						\
      ref_t *MACROp = LOC_TO_PTR((x));		\
						\
      if (OLD_PTR(MACROp))			\
	(x)=loc_touch0((x),0);			\
    }						\
}

#define LOC_TOUCH_PTR(x)				\
{							\
  (x) = LOC_TO_PTR(loc_touch0(PTR_TO_LOC(x),1));	\
}

void
printref(FILE * fd, ref_t refin)
{
  long i;
  char suffix = '?';

  if (refin & PTR_MASK)
    {
      ref_t *p = ANY_TO_PTR(refin);

      if (SPATIC_PTR(p))
	{
	  i = p - spatic.start;
	  suffix = 's';
	}
      else if (NEW_PTR(p))
	{
	  i = p - new_space.start + spatic.size;
	  suffix = 'n';
	}
      else if (OLD_PTR(p))
	{
	  i = p - old_space.start + spatic.size;
	  suffix = 'o';
	}
      else
	i = (long)p >> 2;

      fprintf(fd, "[%ld;tag:%d;%c]", i, refin & TAG_MASK, suffix);
    }
  else
    fprintf(fd, "[%ld;tag:%d]", (long)(refin >> 2), refin & TAG_MASK);
}

#define GC_NULL(r) ((r)==pre_gc_nil || (r)==e_nil)


/* This variant of get_length has to follow forwarding pointers so
   that it will work in the middle of a gc, when an object's type might
   already have been transported. */

static unsigned long
gc_get_length(ref_t x)
{
  if TAG_IS
    (x, PTR_TAG)
    {
      ref_t typ = REF_SLOT(x, 0);
      ref_t vlen_p = REF_SLOT(typ, TYPE_VAR_LEN_P_OFF);
      ref_t len;

      /* Is vlen_p forwarded? */
      if (TAG_IS(vlen_p, LOC_TAG))
	vlen_p = *LOC_TO_PTR(vlen_p);

      /* Is this object variable length? */
      if (GC_NULL(vlen_p))
	{
	  /* Not variable length. */
	  len = REF_SLOT(typ, TYPE_LEN_OFF);

	  /* Is length forwarded? */
	  if (TAG_IS(len, LOC_TAG))
	    len = *LOC_TO_PTR(len);

	  return REF_TO_INT(len);
	}
      else
	return REF_TO_INT(REF_SLOT(x, 1));
    }
  else
    {
      fprintf(stderr, "; WARNING!!!  gc_get_length(");
      printref(stderr, x);
      fprintf(stderr, ") called; only a tag of %d is allowed.\n", PTR_TAG);
      return 0;
    }
}


static ref_t
gc_touch0(ref_t r)
{
  ref_t *p = ANY_TO_PTR(r);

  if (OLD_PTR(p))
    if (r & 1)
      {
	ref_t type_slot = *p;
	if (TAG_IS(type_slot, LOC_TAG))
	  /* Already been transported. */
	  /* Tag magic transforms this:
	     return(PTR_TO_REF(LOC_TO_PTR(type_slot)));
	     to this: */
	  return type_slot | 1L;
	else
	  {
	    /* Transport it */
	    long i;
	    long len = gc_get_length(r);
	    ref_t *new_place = free_point;
	    ref_t *p0 = p;
	    ref_t *q0 = new_place;

	    transport_count += 1;

	    /*
	       fprintf(stderr, "About to transport ");
	       printref(r);
	       fprintf(stderr, " len = %ld.\n", len);
	     */

	    free_point += len;

#ifndef FAST
	    if (free_point >= new_space.end)
	      {
		fprintf(stderr, "\n; New space exhausted while transporting ");
		printref(stderr, r);
		fprintf(stderr, ".\n; This indicates a bug in the garbage collector.\n");
		exit(EXIT_FAILURE);
	      }
#endif
	    for (i = 0; i < len; i++, p0++, q0++)
	      {
		*q0 = *p0;
		*p0 = PTR_TO_LOC(q0);
	      }

	    return (PTR_TO_REF(new_place));
	  }
      }
    else
      {
	/* Follow the chain of locatives to oldspace until we find a
	   real object or a circularity. */
	ref_t r0 = r, r1 = *p, *pp;
	/* int chain_len = 1; */

	while (TAG_IS(r1, LOC_TAG) && (pp = LOC_TO_PTR(r1), OLD_PTR(pp)))
	  {
	    if (r0 == r1)
	      {
		/* fprintf(stderr, "Circular locative chain.\n"); */
		goto forwarded_loc;
	      }
	    r0 = *LOC_TO_PTR(r0);
	    r1 = *pp;
	    /* chain_len += 1; */

	    if (r0 == r1)
	      {
		/* fprintf(stderr, "Circular locative chain.\n"); */
		goto forwarded_loc;
	      }
	    if (!TAG_IS(r1, LOC_TAG) || (pp = LOC_TO_PTR(r1), !OLD_PTR(pp)))
	      break;

	    r1 = *pp;
	    /* chain_len += 1; */
	  }

	/* We're on an object, so touch it. */
	/*
	   fprintf(stderr, "Locative chain followed to ");
	   printref(r1);
	   fprintf(stderr, " requiring %d dereferences.\n", chain_len);
	 */
	GC_TOUCH(r1);
	/* (void)gc_touch(r1); */

	/* Now see if we're looking at a forwarding pointer. */
      forwarded_loc:
	return (r);
      }
  else
    return (r);
}

static ref_t
loc_touch0(ref_t r, bool warn_if_unmoved)
{
  ref_t *p = LOC_TO_PTR(r);

  if (OLD_PTR(p))
    {
      /* A locative into old space.  See if it's been transported yet. */
      ref_t r1 = *p;
      if (TAG_IS(r1, LOC_TAG) && NEW_PTR(LOC_TO_PTR(r1)))
	/* Already been transported. */
	return (r1);
      else
	{
	  /* Better transport this lonely cell. */

	  ref_t *new_place = free_point++;	/* make a new cell. */
	  ref_t new_r = PTR_TO_LOC(new_place);

#ifndef FAST
	  if (free_point >= new_space.end)
	    {
	      fprintf(stderr, "\n; New space exhausted while transporting the cell ");
	      printref(stderr, r);
	      fprintf(stderr, ".\n; This indicates a bug in the garbage collector.\n");
	      exit(EXIT_FAILURE);
	    }
#endif
	  *p = new_r;		/* Record the transportation. */

	  /* Put the right value in the new cell. */

	  *new_place =
	    TAG_IS(r1, PTR_TAG) && (p = REF_TO_PTR(r1), OLD_PTR(p))
	    ? *p | 1 : r1;
	  /* ? PTR_TO_REF(REF_TO_PTR(*p)) : r1; */

	  loc_transport_count += 1;

	  if (warn_if_unmoved)
	    {
	      fprintf(stderr, "\nWarning: the locative ");
	      printref(stderr, r);
	      fprintf(stderr, " has just had its raw cell moved.\n");
	    }
	  return (new_r);
	}
    }
  else
    return (r);			/* Not a locative into old space. */
}


static void
scavenge(void)
{
  ref_t *scavenge_p;

  for (scavenge_p = new_space.start; scavenge_p < free_point; scavenge_p += 1)
    GC_TOUCH(*scavenge_p);
}

static void
loc_scavenge(void)
{
  ref_t *scavenge_p;

  for (scavenge_p = new_space.start; scavenge_p < free_point; scavenge_p += 1)
    LOC_TOUCH(*scavenge_p);
}

#ifndef FAST
/* This set of routines are for consistency checks */

#define GGC_CHECK(r) GC_CHECK(r,"r")

/* True if r seems like a messed up reference. */
static bool
gc_check_(ref_t r)
{
  return (r & PTR_MASK) && !NEW_PTR(ANY_TO_PTR(r))
    && (full_gc || !SPATIC_PTR(ANY_TO_PTR(r)));
}

static void
GC_CHECK(ref_t x, char *st)
{
  if (gc_check_(x))
    {
      fprintf(stderr, "%s = ", st);
      printref(stderr, x);
      if (OLD_PTR(ANY_TO_PTR(x)))
	{
	  fprintf(stderr, ",  cell contains ");
	  printref(stderr, *ANY_TO_PTR(x));
	}
      fprintf(stderr, "\n");
    }
}

static void
GC_CHECK1(ref_t x, char *st, long i)
{
  if (gc_check_((x)))
    {
      fprintf(stderr, (st), (i));
      printref(stderr, x);
      if (OLD_PTR(ANY_TO_PTR(x)))
	{
	  fprintf(stderr, ",  cell contains ");
	  printref(stderr, *ANY_TO_PTR(x));
	}
      fprintf(stderr, "\n");
    }
}
#endif


static u_int16_t *
pc_touch(u_int16_t * o_pc)
{
  ref_t *pcell = (ref_t *) ((unsigned long)o_pc & ~TAG_MASKL);

  /*
    It is possible that the gc was called while a vm was executing the las
    instruction in a code block (hopefully a branch or funcall) in
    a multithreaded enviornment.  So let's back up the pc one before gc'ing
    it.  However, this means the gc general should not be called until the
    loop has read at least one instruction in the code block.
  */
/*pcell--;  Changed my mind.  Moved POLL_GC_SIGNALS to top of loop. */
  LOC_TOUCH_PTR(pcell);
/*pcell++;*/
  return
    (u_int16_t *) ((u_int32_t) pcell
		   | ((u_int32_t) o_pc & TAG_MASK));
}

static void
set_external_full_gc(bool full)
{
  full_gc = full;
}

void
gc (bool pre_dump, bool full_gc, char *reason, size_t amount)
/*
 *     pre_dump        About to dump world?  (discards stacks)
 *     full_gc         Reclaim garbage from spatic space too?
 *     reason          The reason for this GC, human readable.
 *     amount          The amount of space that is needed.
 */
{
  long old_taken;
  ref_t *p;
#ifdef THREADS
  bool ready=false;
  int my_index;
  int i;
  int *my_index_p;
  my_index_p = pthread_getspecific (index_key);
  my_index = *my_index_p;
  gc_ready[my_index] = 1;
  set_gc_flag (true);
#endif

#ifdef THREADS
  /*Problem here is next_index could change if someone creates a thread
    while someone else is gc'ing*/
   while (ready == false) {
    ready = true;
    for (i = 0; i < next_index; i++) {
      if (gc_ready[i] == 0) {
          ready = false;
          break;
      }
    }
  }
#endif

  /* The full_gc flag is also a global to avoid ugly parameter passing. */
  set_external_full_gc(full_gc);

gc_top:
  if (trace_gc == 1)
    fprintf(stderr, "\n;GC");
  if (trace_gc > 1)
    fprintf(stderr, "\n; %sGC due to %s.\n", full_gc ? "Full " : "", reason);

  if (trace_gc > 2 && !pre_dump)
    {
#ifdef THREADS
      for (my_index = 0; my_index < next_index; my_index++) {
#endif
      fprintf (stderr, "value ");
      dump_stack (value_stack_address);
      fprintf (stderr, "context ");
      dump_stack (context_stack_address);
#ifdef THREADS
      }
#endif
    }
  if (trace_gc > 1)
    fprintf(stderr, "; Flipping...");

  old_taken = free_point - new_space.start;
  old_space = new_space;


  if (full_gc)
    new_space.size += spatic.size;
  else
    new_space.size = e_next_newspace_size;

  alloc_space(&new_space, new_space.size);
  free_point = new_space.start;


  transport_count = 0;

  if (trace_gc > 1)
    fprintf(stderr, " rooting...");

  {
    /* Hit the registers: */

    pre_gc_nil = e_nil;
    GC_TOUCH(e_nil);
    GC_TOUCH(e_boot_code);

    if (!pre_dump)
      {

	GC_TOUCH(e_t);
	GC_TOUCH(e_fixnum_type);
	GC_TOUCH(e_loc_type);
	GC_TOUCH(e_cons_type);
	GC_TOUCH_PTR(e_subtype_table, 2);
	/* e_nargs is a fixnum.  Nor is it global... */
	GC_TOUCH (e_env_type);
	GC_TOUCH_PTR (e_argless_tag_trap_table, 2);
	GC_TOUCH_PTR (e_arged_tag_trap_table, 2);
	GC_TOUCH (e_object_type);
	GC_TOUCH (e_segment_type);
#ifdef THREADS
        for (my_index = 0; my_index < next_index; my_index++) {
#endif
	/* e_bp is a locative, but a pointer to the object should exist, so we
	   need only touch it in the locative pass. */
	GC_TOUCH_PTR(e_env, 0);
	GC_TOUCH (e_code_segment);
	GC_TOUCH (e_current_method);
	GC_TOUCH (e_process);
#ifdef THREADS
	}
#endif
	GC_TOUCH (e_uninitialized);
	GC_TOUCH (e_method_type);
	GC_TOUCH (e_operation_type);

	for (p = gc_examine_buffer; p < gc_examine_ptr; p++)
	  GC_TOUCH(*p);




	/* Scan the stacks. */
#ifdef THREADS
        for (my_index=0; my_index<next_index; my_index++) {
#endif
	for (p = value_stack.bp; p <= value_stack.sp; p++)
	  GC_TOUCH(*p);

	for (p = context_stack.bp; p <= context_stack.sp; p++)
	  GC_TOUCH(*p);

	/* Scan the stack segments. */
	GC_TOUCH(value_stack.segment);
	GC_TOUCH(context_stack.segment);
#ifdef THREADS
	}
#endif

	/* Scan static space. */
	if (!full_gc)
	  for (p = spatic.start; p < spatic.end; p++)
	    GC_TOUCH(*p);
      }
    /* Scavenge. */
    if (trace_gc > 1)
      fprintf(stderr, " scavenging...");
    scavenge();

    if (trace_gc > 1)
      fprintf(stderr, " %ld object%s transported.\n",
	      transport_count, transport_count != 1 ? "s" : "");



    /* Clean up the locatives. */
    if (trace_gc > 1)
      fprintf(stderr, "; Scanning locatives...");
    loc_transport_count = 0;

    if (!pre_dump)
      {
#ifdef THREADS
        for (my_index=0; my_index<next_index; my_index++) {
#endif
	LOC_TOUCH_PTR (e_bp);
        e_pc = pc_touch (e_pc);

	LOC_TOUCH(e_uninitialized);

	for (p = gc_examine_buffer; p < gc_examine_ptr; p++)
	  LOC_TOUCH(*p);

	for (p = value_stack.bp; p <= value_stack.sp; p++)
	  LOC_TOUCH(*p);

	for (p = context_stack.bp; p <= context_stack.sp; p++)
	  LOC_TOUCH(*p);
#ifdef THREADS
	}
#endif

	/* Scan spatic space. */
	if (!full_gc)
	  for (p = spatic.start; p < spatic.end; p++)
	    LOC_TOUCH(*p);
      }
    if (trace_gc > 1)
      fprintf(stderr, " scavenging...");
    loc_scavenge();

    if (trace_gc > 1)
      fprintf(stderr, " %ld naked cell%s transported.\n",
	      loc_transport_count, loc_transport_count != 1 ? "s" : "");


    /* Discard weak pointers whose targets have not been transported. */
    if (trace_gc > 1)
      fprintf(stderr, "; Scanning weak pointer table...");
    {
      long count = post_gc_wp();

      if (trace_gc > 1)
	fprintf(stderr, " %ld entr%s discarded.\n",
		count, count != 1 ? "ies" : "y");
    }
  }

#ifndef FAST
  {
    /* Check GC consistency. */

    if (trace_gc > 1)
      fprintf(stderr, "; Checking consistency...\n");

    GGC_CHECK(e_nil);
    GGC_CHECK(e_boot_code);

    if (!pre_dump)
      {
	GGC_CHECK (e_t);
	GGC_CHECK (e_fixnum_type);
	GGC_CHECK (e_loc_type);
	GGC_CHECK (e_cons_type);
	GC_CHECK (PTR_TO_REF (e_subtype_table - 2), "e_subtype_table");
#ifdef THREADS
        for (my_index = 0; my_index < next_index; my_index++) {
#endif
	GC_CHECK (PTR_TO_LOC (e_bp), "PTR_TO_LOC(E_BP)");
	GC_CHECK (PTR_TO_REF (e_env), "e_env");
#ifdef THREADS
        }
#endif
	/* e_nargs is a fixnum.  Nor is it global... */
	GGC_CHECK (e_env_type);
	GC_CHECK (PTR_TO_REF (e_argless_tag_trap_table - 2), "e_argless_tag_trap_table");
	GC_CHECK (PTR_TO_REF (e_arged_tag_trap_table - 2), "e_arged_tag_trap_table");
	GGC_CHECK (e_object_type);
	GGC_CHECK (e_segment_type);
#ifdef THREADS
        for (my_index = 0; my_index < next_index; my_index++) {
#endif
	GGC_CHECK (e_code_segment);
	GGC_CHECK (e_current_method);
	GGC_CHECK (e_process);
#ifdef THREADS
	}
#endif
	GGC_CHECK (e_uninitialized);
	GGC_CHECK (e_method_type);
	GGC_CHECK (e_operation_type);

	/* Scan the stacks. */
#ifdef THREADS
        for (my_index = 0; my_index < next_index; my_index++) {
#endif
	for (p = value_stack.bp; p <= value_stack.sp; p++)
	  GC_CHECK1(*p, "value_stack.bp[%d] = ",
		    (long)(p - value_stack.bp));

	for (p = context_stack.bp; p <= context_stack.sp; p++)
	  GC_CHECK1(*p, "context_stack.bp[%d] = ",
		    (long)(p - context_stack.bp));

	GGC_CHECK(value_stack.segment);
	GGC_CHECK(context_stack.segment);

	/* Make sure the program counter is okay. */
	GC_CHECK ((ref_t) ((ref_t) e_pc | LOC_TAG), "e_pc");
#ifdef THREADS
	}
#endif
      }
    /* Scan the heap. */

    if (!full_gc)
      for (p = spatic.start; p < spatic.end; p++)
	GC_CHECK1(*p, "static_space[%ld] = ", (long)(p - spatic.start));

    for (p = new_space.start; p < free_point; p++)
      GC_CHECK1(*p, "new_space[%ld] = ", (long)(p - new_space.start));
  }
#endif /* not defined(FAST) */

  /* Hopefully there are no more references into old space. */
  free_space(&old_space);

  if (full_gc)
    free_space(&spatic);



#ifdef USE_VADVISE
#ifdef VA_FLUSH
  /* Tell the virtual memory system that recent statistics are useless. */
  vadvise(VA_FLUSH);
#endif
#endif

  if (trace_gc > 2 && !pre_dump)
    {
#ifdef THREADS
      for (my_index = 0; my_index < next_index; my_index++) {
        fprintf (stderr, "Thread %d\n", my_index);
#endif
      fprintf (stderr, "value_stack ");
      dump_stack (value_stack_address);
      fprintf (stderr, "context_stack ");
      dump_stack (context_stack_address);
#ifdef THREADS
      }
#endif
    }
  {
    long new_taken = free_point - new_space.start;
    long old_total = old_taken + (full_gc ? spatic.size : 0);
    long reclaimed = old_total - new_taken;

    if (trace_gc == 1)
      {
	fprintf(stderr, ":%ld%%", (100 * reclaimed) / old_total);
      }
    if (trace_gc > 1)
      {
	fprintf(stderr, "; GC complete.  %ld ", old_total);
	if (full_gc)
	  fprintf(stderr, "(%ld+%ld) ", (long)spatic.size, (long)old_taken);
	fprintf(stderr, "compacted to %ld; %ld (%ld%%) garbage.\n",
		new_taken, reclaimed, (100 * reclaimed) / old_total);
      }

    /* Make the next new space bigger if the current was too small. */
    if (!full_gc && !pre_dump
	&& (RECLAIM_FACTOR * new_taken + amount > new_space.size))
      {
	e_next_newspace_size = RECLAIM_FACTOR * new_taken + amount;
#ifdef MAX_NEW_SPACE_SIZE
	if (e_next_newspace_size > MAX_NEW_SPACE_SIZE)
	  e_next_newspace_size = MAX_NEW_SPACE_SIZE;
#endif
	switch (trace_gc)
	  {
	  case 0:
	    break;
	  case 1:
	    fprintf(stderr, ",resize:%ld", (long)e_next_newspace_size);
	    break;

	  default:
	    fprintf(stderr, "; Expanding next new space from %ld to %ld (%ld%%).\n",
		    (long)new_space.size, (long)e_next_newspace_size,
		    (long)(100 * (e_next_newspace_size - new_space.size))
		    / new_space.size);
	    break;
	  }

	if ((size_t) (new_space.end - free_point) < amount)
	  {
#ifdef MAX_NEW_SPACE_SIZE
	    if (((new_space.end - free_point) + amount) < e_next_newspace_size)
	      {
		fprintf(stderr, "\nFatal GC error: Essential new space size exceeds maximum allowable.\n");
		exit(EXIT_FAILURE);
	      }
#endif
	    reason = "immediate new space expansion necessity";
	    goto gc_top;
	  }
      }
    if (full_gc && !pre_dump)
      {
	/* Ditch old spatic, move _new to spatic, and reallocate new. */
	/* This is a bug
	   free_space (&spatic);
	 */
	spatic = new_space;
	realloc_space(&spatic, free_point - new_space.start);

	if (trace_gc > 1 && e_next_newspace_size != original_newspace_size)
	  fprintf(stderr, "; Setting new space size to %ld.\n",
		  (long)original_newspace_size);
	new_space.size = e_next_newspace_size = original_newspace_size;
	if (e_next_newspace_size <= amount)
	  {
	    e_next_newspace_size = RECLAIM_FACTOR * amount;
	    switch (trace_gc)
	      {
	      case 0:
		break;
	      case 1:
		fprintf(stderr, ",resize:%ld", (long)e_next_newspace_size);
		break;
	      default:
		fprintf(stderr,
			"; expanding next new space %ld to %ld (%ld%%).\n",
			(long)new_space.size, (long)e_next_newspace_size,
			(long)(100 * (e_next_newspace_size - new_space.size)) / new_space.size);
		break;
	      }
	    new_space.size = e_next_newspace_size;
	  }
	alloc_space(&new_space, new_space.size);
	free_point = new_space.start;
      }
    if (trace_gc == 1)
      fprintf(stderr, "\n");
    if (trace_gc)
      fflush(stdout);
  }
#ifdef THREADS
    my_index_p = pthread_getspecific (index_key);
    my_index = *my_index_p;
    gc_ready[my_index] = 0;
    set_gc_flag (false);
#endif 
}



/* This routine takes a block of memory and scans through it, updating
   all pointers into the window starting at old_start to instead point
   into the corresponding location in new_start.  Typically new_start
   will be the same as start */

void
shift_targets(ref_t * start, size_t len,
	      ref_t * old_start, size_t old_len,
	      ref_t * new_start)
{
  size_t i;
  for (i = 0; i < len; i++)
    {
      ref_t x = start[i];
      if (PTR_MASK & x)		/* is it a pointer? */
	{
	  ref_t *y = ANY_TO_PTR(x);
	  size_t offset = y - old_start;
	  if (y >= 0 && offset < old_len)	/* into old window? */
	    start[i] = PTR_TO_TAGGED(new_start + offset, x);
	}
    }
}
