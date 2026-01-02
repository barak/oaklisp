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


#ifndef _DATA_H_INCLUDED
#define _DATA_H_INCLUDED

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "config.h"
#include "threads.h"

/* bool with pointer passed to getopt */

typedef int bool_int;

/* reference type */

typedef size_t ref_t;

/* instruction type */

typedef u_int16_t instr_t;

/* space type */

typedef struct {
  ref_t *start;
  ref_t *end;
  size_t size;		/* in size reference_t */
} space_t;


extern space_t new_space, old_space, spatic;
extern ref_t *free_point;

/* Size of first newspace, in K */
#define DEFAULT_NEWSPACE 128

/* The following is for stack debugging */
#define PATTERN 0x0a0b0c0d


#define CONTEXT_FRAME_SIZE 3	/* not a tunable parameter */


/* Garbage collection */

#define GC_EXAMINE_BUFFER_SIZE 16
#ifdef THREADS
extern ref_t gc_examine_buffer_array[MAX_THREAD_COUNT][GC_EXAMINE_BUFFER_SIZE];
extern ref_t *gc_examine_ptr_array[MAX_THREAD_COUNT];
#define gc_examine_buffer	gc_examine_buffer_array[my_index]
#define gc_examine_ptr		gc_examine_ptr_array[my_index]
#else
extern ref_t gc_examine_buffer[GC_EXAMINE_BUFFER_SIZE];
extern ref_t *gc_examine_ptr;
#endif

/* Virtual Machine registers */

typedef struct {
  ref_t     *e_bp;
  ref_t     *e_env;
  ref_t     e_current_method;
  ref_t     e_code_segment;
  instr_t *e_pc;
  unsigned  e_nargs;
  ref_t     e_process;
} register_set_t;


#ifdef THREADS
extern ref_t
  e_t, e_nil, e_fixnum_type, e_loc_type, e_cons_type, e_env_type,
 *e_subtype_table, e_object_type, e_segment_type, e_boot_code,
 *e_arged_tag_trap_table, *e_argless_tag_trap_table,
  e_uninitialized, e_method_type, e_operation_type;
#else
extern ref_t
 *e_bp, *e_env, e_t, e_nil, e_fixnum_type, e_loc_type, e_cons_type, e_env_type,
 *e_subtype_table, e_object_type, e_segment_type, e_boot_code, e_code_segment,
 *e_arged_tag_trap_table, *e_argless_tag_trap_table, e_current_method,
  e_uninitialized, e_method_type, e_operation_type, e_process;

extern instr_t *e_pc;

extern unsigned e_nargs;
#endif

#define e_false e_nil

extern size_t e_next_newspace_size, original_newspace_size;

extern char *world_file_name;
extern char *dump_file_name;
extern int dump_base;

extern bool dump_flag;
extern bool gc_before_dump;

extern int trace_gc;
extern bool_int trace_traps;
extern bool_int batch_mode;

#ifndef FAST

extern bool_int trace_insts;
extern bool_int trace_segs;
extern bool_int trace_valcon;
extern bool_int trace_cxtcon;
extern bool_int trace_stks;
extern bool_int trace_meth;

#ifdef OP_TYPE_METH_CACHE
extern bool_int trace_mcache;
#endif

#endif

extern bool_int trace_files;


/* miscellanous */

#ifndef ISATTY
#define ISATTY(stream) (isatty(fileno(stream)))
#endif


#define READ_MODE "r"
#define WRITE_MODE "w"
#define APPEND_MODE "a"
#define READ_BINARY_MODE "rb"
#define WRITE_BINARY_MODE "wb"



/* Tag Scheme */

#define SIGN_16BIT_ARG(x)	((int16_t)(x))

#define TAGSIZE 2

#define TAG_MASK	3
#define TAG_MASKL	3l
#define SUBTAG_MASK	0xff
#define SUBTAG_MASKL	0xffl

#define INT_TAG		0
#define IMM_TAG		1
#define LOC_TAG 	2
#define PTR_TAG		3

#define PTR_MASK	2

#define CHAR_SUBTAG	 IMM_TAG

#define TAG_IS(X,TAG)		(((X)&TAG_MASK)==(TAG))
#define SUBTAG_IS(X,SUBTAG)	(((X)&SUBTAG_MASK)==(SUBTAG))


/* #define OR_TAG */

#define REF_TO_INT(r)   ((int32_t)r>>TAGSIZE)


#define REF_TO_PTR(r)	((ref_t*)((r)-PTR_TAG))
/*
   #define REF_TO_PTR(r)   ((ref_t*)((r)&~3ul))
 */
/* This maybe used in slot calculations, where tag corrections
   can be done by the address calculation unit */
#define REF_TO_PTR_ADDR(r) ((ref_t*)((r) - PTR_TAG))


#define LOC_TO_PTR(r)	((ref_t*)((r) - LOC_TAG))
#define ANY_TO_PTR(r)	((ref_t*)((r) & ~TAG_MASKL))

#ifndef OR_TAG
#define PTR_TO_LOC(p)	((ref_t)((ref_t)(p) + LOC_TAG))
#define PTR_TO_REF(p)	((ref_t)((ref_t)(p) + PTR_TAG))
#else
#define PTR_TO_LOC(p)   ((ref_t)((ref_t)(p) | LOC_TAG))
#define PTR_TO_REF(p)   ((ref_t)((ref_t)(p) | PTR_TAG))
#endif

/* Put q's tag onto p */
#define PTR_TO_TAGGED(p,q) ((ref_t)((ref_t)(p) + ((q) & TAG_MASK)))

#define REF_TO_CHAR(r)	((char)((r)>>8))
#ifndef OR_TAG
#define CHAR_TO_REF(c)	(((ref_t)(c)<<8) + CHAR_SUBTAG)
#else
#define CHAR_TO_REF(c)  (((ref_t)(c)<<8) | IMM_TAG)
#endif

#ifndef OR_TAG
#define INT_TO_REF(i)	((ref_t)(((int32_t)(i)<<TAGSIZE) + INT_TAG))
#else
#define INT_TO_REF(i)   ((ref_t)(((int32_t)(i)<<TAGSIZE) | INT_TAG))
#endif

#define BOOL_TO_REF(x)   ( (x) ? e_t : e_false )

/* MIN_REF is the most negative fixnum.  There is no corresponding
   positive fixnum, an asymmetry inherent in a twos complement
   representation. */

#define MIN_REF     ((ref_t)((ref_t)0x1<<(__WORDSIZE-1)))
#define MAX_REF     ((ref_t)-((ssize_t)MIN_REF+1))

/* Check if high three bits are equal. */

/*
#define OVERFLOWN_INT(i,code)					\
{ register int highcrap						\
	= ((u_int32_t)(i)) >> (__WORDSIZE-(TAGSIZE+1));		\
if ((highcrap != 0x0) && (highcrap != 0x7)) {code;} }
*/

/* The following is for 32-bit ref_t only */

#define OVERFLOWN_INT(i,code)					\
{ u_int32_t highcrap = (i) & 0xe0000000;			\
if ((highcrap) && (highcrap != 0xe0000000)) {code;}}

/*
 * Offsets for wired types.  Offset includes type and
 * optional length fields when present.
 */

/* CONS-PAIR: */
#define CONS_PAIR_CAR_OFF	1
#define CONS_PAIR_CDR_OFF	2

/* TYPE: */
#define TYPE_LEN_OFF		1
#define TYPE_VAR_LEN_P_OFF	2
#define TYPE_SUPER_LIST_OFF	3
#define TYPE_IVAR_LIST_OFF	4
#define TYPE_IVAR_COUNT_OFF	5
#define TYPE_TYPE_BP_ALIST_OFF	6
#define TYPE_OP_METHOD_ALIST_OFF 7
#define TYPE_WIRED_P_OFF	8

/* METHOD: */
#define METHOD_CODE_OFF		1
#define METHOD_ENV_OFF		2

/* CODE-VECTOR: */
#define CODE_IVAR_MAP_OFF	2
#define CODE_CODE_START_OFF	3

/* OPERATION: */
#define OPERATION_LAMBDA_OFF		1
#define OPERATION_CACHE_TYPE_OFF	2
#define OPERATION_CACHE_METH_OFF	3
#define OPERATION_CACHE_TYPE_OFF_OFF	4
#define OPERATION_LENGTH		5

/* ESCAPE-OBJECT */
#define ESCAPE_OBJECT_VAL_OFF	1
#define ESCAPE_OBJECT_CXT_OFF	2

/* Continuation Objects */
#define CONTINUATION_VAL_SEGS	1
#define CONTINUATION_VAL_OFF	2
#define CONTINUATION_CXT_SEGS	3
#define CONTINUATION_CXT_OFF	4

#define SPACE_PTR(s,p)	((s).start<=(p) && (p)<(s).end)
#define NEW_PTR(r)      SPACE_PTR(new_space,(r))
#define SPATIC_PTR(r)	SPACE_PTR(spatic,(r))
#define OLD_PTR(r) (SPACE_PTR(old_space,(r))||(full_gc&&SPACE_PTR(spatic,(r))))




/* Leaving r unsigned lets us checks for negative and too big in one shot: */
#define wp_to_ref(r)					\
  ( (u_int32_t)REF_TO_INT(r) >= (u_int32_t) wp_index ?	\
   e_nil : wp_table[1+(u_int32_t)REF_TO_INT((r))] )


/* This is used to allocate some storage.  It calls gc when necessary. */

#define ALLOCATE(p, words, reason)			\
  ALLOCATE_PROT(p, words, reason,; ,; )

/* This is used to allocate some storage */

#define ALLOCATE_SS(p, words, reason)			\
  ALLOCATE_PROT(p, words, reason,			\
		{ value_stack.sp = local_value_sp;	\
          context_stack.sp = local_context_sp;		\
		  e_pc = local_e_pc; },			\
		{ local_e_pc = e_pc;			\
          local_context_sp = context_stack.sp;		\
		  local_value_sp = value_stack.sp; })


/* This allocates some storage, assuming that v must be protected from gc. */

#define ALLOCATE1(p, words, reason, v)			\
  ALLOCATE_PROT(p, words, reason,			\
		{ GC_MEMORY(v);				\
		  value_stack.sp = local_value_sp;	\
          context_stack.sp = local_context_sp;		\
		  e_pc = local_e_pc; },			\
		{ local_e_pc = e_pc;			\
          local_context_sp = context_stack.sp;		\
		  local_value_sp = value_stack.sp;	\
		  GC_RECALL(v); })


#define ALLOCATE_PROT(p, words, reason, before, after)	\
{							\
  THREADY(						\
      while (pthread_mutex_trylock(&alloc_lock) != 0) {	\
	      if (gc_pending) {				\
		      before; wait_for_gc(); after;	\
	      }						\
      }							\
  )							\
  if (free_point + (words) >= new_space.end)            \
    {							\
      before;						\
      gc(false, false, (reason), (words));		\
      after;						\
    }							\
  (p) = free_point;					\
  free_point += (words);				\
  THREADY( pthread_mutex_unlock (&alloc_lock); )	\
}

/* These get slots out of Oaklisp objects, and may be used as lvalues. */

#define SLOT(p,s)	(*((p)+(s)))
#define REF_SLOT(r,s)	SLOT(REF_TO_PTR(r),s)


/* This is for the warmup code */

#define CODE_SEG_FIRST_INSTR(seg) \
  ( (instr_t *)(REF_TO_PTR((seg)) + CODE_CODE_START_OFF) )


#ifdef THREADS

#define reg_set register_array[my_index]
#define value_stack (*value_stack_array[my_index])
#define context_stack (*cntxt_stack_array[my_index])
#define value_stack_address value_stack_array[my_index]
#define context_stack_address cntxt_stack_array[my_index]
#define e_code_segment     ( (reg_set->e_code_segment) )
#define e_current_method   ( (reg_set->e_current_method) )
#define e_pc               ( (reg_set->e_pc) )
#define e_bp               ( (reg_set->e_bp) )
#define e_env              ( (reg_set->e_env) )
#define e_nargs            ( (reg_set->e_nargs) )
#define e_process          ( (reg_set->e_process) )

#else

extern register_set_t *reg_set;
#define value_stack_address &value_stack
#define context_stack_address &context_stack

#endif

extern int create_thread(ref_t start_method);

extern register_set_t* register_array[];

#endif
