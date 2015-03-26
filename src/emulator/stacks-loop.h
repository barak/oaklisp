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


#ifndef STACKS_LOOP_INCLUDED
#define STACKS_LOOP_INCLUDED

#include "stacks.h"

#define LOCALIZE_VAL()					\
{	local_value_sp = value_stack.sp;		\
}

#define UNLOCALIZE_VAL()				\
{	value_stack.sp = local_value_sp;		\
}

#define LOCALIZE_CXT()					\
{	local_context_sp = context_stack.sp;		\
}

#define UNLOCALIZE_CXT()				\
{	context_stack.sp = local_context_sp;		\
}

#define LOCALIZE_REGS()					\
{	local_epc = e_pc;				\
}

#define UNLOCALIZE_REGS()				\
{	e_pc = local_epc;				\
}

#define LOCALIZE_STKS()					\
{	LOCALIZE_VAL();					\
	LOCALIZE_CXT();					\
}

#define UNLOCALIZE_STKS()				\
{	UNLOCALIZE_VAL();				\
	UNLOCALIZE_CXT();				\
}

#define LOCALIZE_ALL()					\
{	LOCALIZE_STKS();				\
	LOCALIZE_REGS();				\
}

#define UNLOCALIZE_ALL()				\
{	UNLOCALIZE_STKS();				\
	UNLOCALIZE_REGS();				\
}

#define VALUE_FLUSH(amount_to_leave)			\
{	UNLOCALIZE_ALL();				\
	stack_flush(&value_stack, (amount_to_leave));	\
	LOCALIZE_ALL();					\
}

#define CONTEXT_FLUSH(amount_to_leave)			\
{	UNLOCALIZE_ALL();				\
	stack_flush(&context_stack, (amount_to_leave));	\
	LOCALIZE_ALL();					\
}


#define VALUE_UNFLUSH(n) 			\
{	UNLOCALIZE_VAL();			\
	stack_unflush(&value_stack, (n));	\
	LOCALIZE_VAL();				\
}

#define CONTEXT_UNFLUSH(n) 			\
{	UNLOCALIZE_CXT();			\
	stack_unflush(&context_stack, (n));	\
	LOCALIZE_CXT();				\
}


#define DUMP_VALUE_STACK() 			\
{	UNLOCALIZE_VAL();			\
	dump_stack(&value_stack);		\
}

#define DUMP_CONTEXT_STACK() 			\
{	UNLOCALIZE_CXT();			\
	dump_stack(&context_stack);		\
}

#define VALUE_STACK_HEIGHT() 				\
	    (local_value_sp - value_stack_bp + 1 	\
         + value_stack.pushed_count)

#define CONTEXT_STACK_HEIGHT() 				\
	    (local_context_sp - context_stack_bp + 1 	\
         + context_stack.pushed_count)

/* The top of stack is always visible.
   Therefore PEEKVAL() can be used as an lvalue. */

#define PEEKVAL()	(*local_value_sp)

/* When you are sure that the buffer has enough elements in it,
   use this for looking deeper into the stack */
#define PEEKVAL_UP(x)	(*(local_value_sp-(x)))

/* Use these when you are sure that overflows and underflows cannot occur. */
#define PUSHVAL_NOCHECK(r)  { *++local_value_sp = (r); }
#define POPVAL_NOCHECK()    (*local_value_sp--)


#define PUSHVAL(r)					\
{							\
  if (local_value_sp+1 < value_stack_end)		\
    { *++local_value_sp = (r); }			\
  else {						\
        GC_MEMORY(r);					\
	VALUE_FLUSH(value_stack.filltarget);		\
	GC_RECALL(*++local_value_sp);			\
  }							\
}

#define PUSHVAL_IMM(r)					\
{							\
	CHECKVAL_PUSH(1);				\
	PUSHVAL_NOCHECK((r));				\
}

#define POPVAL(v)					\
{							\
	CHECKVAL_POP(1);				\
	(v) = *local_value_sp--;			\
}

/* The following routines check that n elements can be pushed
   without overflow */

#define CHECKVAL_PUSH(n)				\
{	if (&local_value_sp[(n)] >= value_stack_end)	\
	  VALUE_FLUSH(value_stack.filltarget);		\
}

#define CHECKCXT_PUSH(n)				\
{	if (&local_context_sp[(n)] >=			\
	    context_stack_end)				\
	  CONTEXT_FLUSH(context_stack.filltarget);	\
}

/* The following check that n elements can be popped without underflow. */
#define CHECKVAL_POP(n)						\
{	if (&local_value_sp[-(n)] < value_stack_bp)		\
		VALUE_UNFLUSH((n));				\
}

#define CHECKCXT_POP(n)						\
{	if (&local_context_sp[-(n)] < context_stack_bp)		\
		CONTEXT_UNFLUSH((n));				\
}

/* This routine avoids having a bogus reference in the segments */
#define BASH_SEGMENT_TYPE()					\
{	value_stack.segment = e_nil;				\
	context_stack.segment = e_nil;				\
}

/* This pops some elements off the value stack.
   It is inefficient because it copies elements into the buffer
   and then pops them off.  A better thing should be written.  */
#define POPVALS(n)						\
{	CHECKVAL_POP((n));					\
	local_value_sp -= (n);					\
}

#define POPCXTS(n)						\
{	CHECKCXT_POP((n));					\
	local_context_sp -= (n);				\
}

#define PUSH_CONTEXT(off_set)					\
{								\
	CHECKCXT_PUSH(CONTEXT_FRAME_SIZE);			\
	local_context_sp[1] = INT_TO_REF((unsigned long)local_epc - \
	(unsigned long)e_code_segment +((off_set)<<1));		\
	local_context_sp[2] = e_current_method;			\
	local_context_sp[3] = PTR_TO_LOC(e_bp);			\
	local_context_sp += 3;					\
}

#define POP_CONTEXT()						\
{								\
	CHECKCXT_POP(CONTEXT_FRAME_SIZE);			\
	e_bp = LOC_TO_PTR(local_context_sp[0]);			\
	e_current_method = local_context_sp[-1];       		\
	e_env = REF_TO_PTR(e_current_method);			\
	e_code_segment = SLOT(e_env,METHOD_CODE_OFF);		\
	e_env = REF_TO_PTR(SLOT(e_env,METHOD_ENV_OFF));		\
	local_epc = (instr_t *)			        \
				((unsigned long)e_code_segment	\
				+REF_TO_INT(local_context_sp[-2])); \
	local_context_sp -= 3;					\
}

#define BASH_VAL_HEIGHT(h)					\
{	int to_pop = VALUE_STACK_HEIGHT()-(h);			\
	POPVALS(to_pop);					\
}

#define BASH_CXT_HEIGHT(h)					\
{	int to_pop = CONTEXT_STACK_HEIGHT()-(h);		\
	POPCXTS(to_pop);					\
}

#define MAKE_BACK_VAL_PTR(v,dist)			\
{	CHECKVAL_POP((dist));				\
	(v) = local_value_sp - (dist);			\
}

#endif
