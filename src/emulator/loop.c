/*********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.   *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                    *
 *     Distributed under the GNU General Public License v2 or later  *
 *********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef FAST
#undef NDEBUG
#endif
#include <assert.h>

#include "config.h"
#include "data.h"
#include "stacks.h"
#include "gc.h"
#include "signal.h"
#include "timers.h"
#include "weak.h"
#include "worldio.h"
#include "loop.h"
#include "cmdline.h"
#include "xmalloc.h"


#ifdef _ICC
#pragma IMS_linkage ("section%loop")
#endif

#define ENABLE_TIMER	0


#ifdef FAST

#define trace_insts	0
#define trace_stcon	0
#define trace_cxcon	0
#define trace_meth	0
#define trace_segs	0
#define trace_mcache	0

#else

bool trace_insts = false;	/* trace instruction execution */
bool trace_stcon = false;	/* trace stack contents */
bool trace_cxcon = false;	/* trace contents stack contents */
bool trace_meth = false;	/* trace method lookup */
bool trace_segs = false;	/* trace stack segment manipulation */
bool trace_mcache = false;	/* trace method cache hits and misses */

#endif


bool trace_traps = false;	/* trace tag traps */
bool trace_files = false;	/* trace file opening */


bool gc_before_dump = true;	/* do a GC before dumping the world */


#ifdef FAST
#define MAYBE_PUT(v,s)
#else
#define MAYBE_PUT(v,s) { if ((v)) {(void)printf(s); fflush(stdout);} }
#endif



#define NEW_STORAGE e_uninitialized

static void
maybe_dump_world (int dumpstackp)
{
  if (dumpstackp > 2)
    {				/* 0,1,2 are normal exits. */
      /* will be changed */
      printf ("value stack");
      dump_stack (&value_stack);
      printf ("context stack");
      dump_stack (&context_stack);
    }
  if (dump_flag)
    {
      if (gc_before_dump && dumpstackp == 0)
	{
	  gc (true, true, "impending world dump", 0);
	  dump_world (true);
	}
      else
	dump_world (false);
    }
}


#define TRACEMETHOD(zz) {if (trace_meth) {printf("meth-trace%ld  ",zz);          \
					 printref("obj_type:%ld~%ld  ",obj_type); \
   				         printref("alist:%ld~%ld  ",alist);       \
				         printref("mptr:%ld~%ld\n",*method_ptr);  }}

#define TRACEASSQ(zz) {if (trace_meth) {printf("aq-trace%ld  ",zz);          \
   				         printref("elem:%ld~%ld  ",elem);       \
				         printref("list:%ld~%ld\n",list);  }}

#define TRACEPASSQ(zz) {printf("aq-trace%ld  ",zz);    \
			  printref("elem:%ld~%ld  ",elem);       \
			    printref("l:%ld~%ld ",l);  \
			    printref("cdr(l):%ld~%ld ",cdr(l));  \
			    if (locl) printref("*locl:%ld~%ld\n",*locl); }


#define get_type(x)							  \
((x)&1 ?								  \
 ((x)&2 ? REF_SLOT(x, 0) : *(e_subtype_table + ((x&SUBTAG_MASK)/4))) : \
 ((x)&2 ? e_loc_type : e_fixnum_type))



ref_t later_lists[100];

static void
find_method_type_pair (ref_t op,
		       ref_t obj_type,
		       ref_t * method_ptr,
		       ref_t * type_ptr)
/************************************************/
{
  ref_t alist;
  ref_t *locl = 0;
  ref_t thelist;
  ref_t *loclist;
  ref_t *llp = &later_lists[-1];

  while (1)			/* forever */
    {
      /* First look for it in the local method alist of obj_type: */

      alist = thelist =
	*(loclist = &REF_SLOT (obj_type, TYPE_OP_METHOD_ALIST_OFF));

      while (alist != e_nil)
	{
	  if (car (car (alist)) == op)
	    {
	      if (locl != 0)
		{
		  *locl = cdr (alist);
		  *loclist = alist;
		  cdr (alist) = thelist;
		}
	      *method_ptr = cdr (car (alist));
	      *type_ptr = obj_type;
	      return;
	    }
	  alist = *(locl = &cdr (alist));
	}

      /* Not there, stack the supertype list and then fetch the top guy
         available from the stack. */

      *++llp = REF_SLOT (obj_type, TYPE_SUPER_LIST_OFF);

      while (*llp == e_nil)
	{
	  if (llp == later_lists)
	    return;		/* Nothing. */
	  llp--;
	}

      locl = 0;
      obj_type = car (*llp);
      *llp = cdr (*llp);
    }
}



void
loop(void)
{
  /* This is used for instructions to communicate with 
     the trap code, when a fault is encountered. */
  unsigned trap_nargs;
  u_int16_t instr;
  u_int8_t op_field;
  u_int8_t arg_field;
  ref_t x=INT_TO_REF(0), y; /* initialized to turn off -Wall message */

  ref_t *local_value_sp = value_stack.sp;
  ref_t *local_context_sp = context_stack.sp;
  ref_t *const value_stack_end
  = &value_stack.bp[value_stack_size];
  u_int16_t *local_epc = e_pc;

#if ENABLE_TIMER
  unsigned timer_counter = 0;
#endif


  /* This fixes a bug in which the initial CHECK-NARGS 
     in the boot code tries to pop the operation and fails. */
  PUSHVAL_IMM (INT_TO_REF (4321));

  /* These TRAPx(n) macros jump to the trap code, notifying it that x
     arguments have been popped off the stack and need to be put back
     on (these are in the variables x, ...) and that the trap operation
     should be called with the top n guys on the stack as arguments. */


#define TRAP0(N) {trap_nargs=((N)); goto arg0_tt;}
#define TRAP1(N) {trap_nargs=((N)); goto arg1_tt;}

#define TRAP0_IF(C,N) {if ((C)) TRAP0((N));}
#define TRAP1_IF(C,N) {if ((C)) TRAP1((N));}

#define CHECKTAG0(X,TAG,N) TRAP0_IF(!TAG_IS((X),(TAG)),(N))
#define CHECKTAG1(X,TAG,N) TRAP1_IF(!TAG_IS((X),(TAG)),(N))

#define CHECKCHAR0(X,N) \
    TRAP0_IF(!SUBTAG_IS((X),CHAR_SUBTAG),(N))
#define CHECKCHAR1(X,N) \
    TRAP1_IF(!SUBTAG_IS((X),CHAR_SUBTAG),(N))

#define CHECKTAGS1(X0,T0,X1,T1,N) \
    TRAP1_IF( !TAG_IS((X0),(T0)) || !TAG_IS((X1),(T1)), (N))

#define CHECKTAGS_INT_1(X0,X1,N) \
    TRAP1_IF( (((X0)|(X1)) & TAG_MASK) != 0, (N))


#ifndef _ICC
#define POLL_USER_SIGNALS()	if (signal_poll_flag) {goto intr_trap;}
#else
#define POLL_USER_SIGNALS()
#endif

#if ENABLE_TIMER
#define TIMEOUT	200000
#define POLL_TIMER_SIGNALS()	if (timer_counter > TIMEOUT) {goto intr_trap;}
#else
#define POLL_TIMER_SIGNALS()
#endif

#define POLL_SIGNALS()		POLL_USER_SIGNALS() ;		\
				POLL_TIMER_SIGNALS()

  /* This is the big instruction fetch/execute loop. */

#ifndef _ICC
  enable_signal_polling ();
#endif

#ifndef _ICC
#define GOTO_TOP	goto top_of_loop;
#elif defined __GNUC__
#define GOTO_TOP        break;
#else 
/* this is a compiler bug in the Inmos compiler */
#define GOTO_TOP    break;
#endif



 top_of_loop:
  while (1)		/* forever */
    {

     /*
       extern const char * ArglessInstrs[];
       extern const char * Instrs[];
     */
#ifndef FAST
     if (trace_stcon)
       DUMP_VALUE_STACK ();
     if (trace_cxcon)
       DUMP_CONTEXT_STACK ();
#endif

#if ENABLE_TIMER
   /*
     if (++timer_counter > 200000) {
	 timer_counter = 0;
	 printf("*Bing*");
     }
   */
     timer_counter++;
#endif

     instr = *local_epc++;


#if BYTE_GENDER == little_endian
     /* this is little endian code, work it out for big endian,
	if you like */
     arg_field = (unsigned char) (((u_int8_t *) &instr)[1]);
#else
     arg_field = (instr >> 8);
#endif

#if 0 && BYTE_GENDER == little_endian
     /* this is currently not used, little endian code */
#define signed_arg_field  ((int)(((signed char*)&instr)[1]))
#else
     /* Watcom C++ */
#define signed_arg_field  ((int)(short)instr>>8)
#endif

     op_field = ((u_int8_t)instr >> 2);

     /*
       fprintf(stdout, "Asserting...\n");
       assert(value_stack_bp[-1] == PATTERN);
       assert(value_stack_bp[value_stack_size] == PATTERN);
       assert(context_stack_bp[-1] == PATTERN);
       assert(context_stack_bp[context_stack_size] == PATTERN);
     */

     /* old disassembler, subtract 1 from local_epc now */
     /*
       if(SPATIC_PTR ((ref_t*) local_epc))
       fprintf(stdout, "address:%#.5ld[spatic]: ", 
       (char*) local_epc - (char *) spatic.start);
       else
       fprintf(stdout, "address:%#.5ld[new   ]: ",
       (char*) local_epc - (char *) new_space.start
       + 4 * spatic.size);
       if (!op_field)
       fprintf(stdout, "%.20s, opcode:%.2d\n",
       ArglessInstrs[arg_field], arg_field);
       else
       fprintf(stdout, "%.20s, opcode:%.2d, arg:%3d\n",
       Instrs[op_field], op_field, arg_field);
       fflush(stdout);
       getchar();    
     */

     if (op_field == 0)
       {
	 switch (arg_field)
	   {

	   case 0:		/* NOOP */
	     GOTO_TOP;

	   case 1:		/* PLUS */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     {
	       long a = REF_TO_INT (x) + REF_TO_INT (y);
	       OVERFLOWN_INT (a, TRAP1 (2));
	       PEEKVAL() = INT_TO_REF (a);
	     }
	     GOTO_TOP;

	   case 2:		/* NEGATE */
	     x = PEEKVAL();
	     CHECKTAG0 (x, INT_TAG, 1);
	     /* The most negative fixnum's negation isn't a fixnum. */
	     if (x == MIN_REF)
	       TRAP0(1);
	     /* Tag trickery: */
	     PEEKVAL() = -((long) x);
	     GOTO_TOP;

	   case 3:		/* EQ? */
	     POPVAL(x);
	     if (x == PEEKVAL())
	       {
		 PEEKVAL() = e_t;
		 GOTO_TOP;
	       }
	     else
	       {
		 PEEKVAL() = e_nil;
		 GOTO_TOP;
	       }

	   case 4:		/* NOT */
	     if (PEEKVAL() == e_nil)
	       {
		 PEEKVAL() = e_t;
		 GOTO_TOP;
	       }
	     else
	       {
		 PEEKVAL() = e_nil;
		 GOTO_TOP;
	       }

	   case 5:		/* TIMES */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     /* Tag trickery: */
#ifdef HAVE_LONG_LONG
	     {
	       long long a = (long long )REF_TO_INT (x) * (long long) y;
	       long highcrap = a >> (WORDSIZE - 2);
	       if ((highcrap) && (highcrap != -1L))
		 TRAP1 (2);
	       PEEKVAL() = (ref_t) a;
	       
	     }

#elif defined(DOUBLES_FOR_OVERFLOW)
	     {
	       double a = (double) REF_TO_INT (x) * (double) REF_TO_INT (y);
	       if (a < (double) ((long) MIN_REF / 4)
		   || a > (double) ((long) MAX_REF / 4))
		 TRAP1 (2);
	       PEEKVAL() = INT_TO_REF ((long) a);
	     }
#else
	     {
	       long a = REF_TO_INT (x), b = REF_TO_INT (y);
	       unsigned long al, ah, bl, bh, hh, hllh, ll;
	       long answer;
	       bool neg = false;
	       /* MNF check */
	       if (a < 0)
		 {
		   a = -a;
		   neg = true;
		 }
	       if (b < 0)
		 {
		   b = -b;
		   neg = !neg;
		 }
	       al = a & 0x7FFF;
	       bl = b & 0x7FFF;
	       ah = (unsigned long) a >> 15;
	       bh = (unsigned long) b >> 15;
	       ll = al * bl;
	       hllh = al * bh + ah * bl;
	       hh = ah * bh;
	       if (hh || hllh >> 15)
		 TRAP1 (2);
	       answer = (hllh << 15) + ll;
	       OVERFLOWN_INT (answer, TRAP1 (2));
	       PEEKVAL() = INT_TO_REF (neg ? -answer : answer);
	     }
#endif
	     GOTO_TOP;

	   case 6:		/* LOAD-IMM ; INLINE-REF */
	     /* align pc to next word boundary: */

	     if ((unsigned long) local_epc & 2)
	       local_epc++;
	     /*NOSTRICT */
	     PUSHVAL(*(ref_t *) local_epc);
	     local_epc += sizeof (ref_t) / sizeof (*local_epc);
	     GOTO_TOP;

	   case 7:		/* DIV */
	     /* Sign of product of args. */
	     /* Round towards 0.  Obeys identity w/ REMAINDER. */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     /* Can't divide by 0, or the most negative fixnum by -1. */
	     if (y == INT_TO_REF (0) ||
		 (y == INT_TO_REF (-1) && x == MIN_REF))
	       TRAP1 (2);
	     /* Tag trickery: */
	     PEEKVAL() = INT_TO_REF ((long) x / (long) y);
	     GOTO_TOP;

	   case 8:		/* =0? */
	     x = PEEKVAL();
	     CHECKTAG0 (x, INT_TAG, 1);

	     if (x == INT_TO_REF (0))
	       {
		 PEEKVAL() = e_t;
		 GOTO_TOP;
	       }
	     else
	       {
		 PEEKVAL() = e_nil;
		 GOTO_TOP;
	       }


	   case 9:		/* GET-TAG */
	     PEEKVAL() = INT_TO_REF (PEEKVAL() & TAG_MASK);
	     GOTO_TOP;

	   case 10:		/* GET-DATA */

	     /* With the moving gc, this should *NEVER* be used.

		For ease of debugging with the multiple spaces, this
		makes it seem like spatic and _new spaces are contiguous,
		is compatible with print_ref, and also with CRUNCH. */
	     x = PEEKVAL();
	     if (x & PTR_MASK)
	       {
		 ref_t *p = (x & 1) ? REF_TO_PTR (x) : LOC_TO_PTR (x);

		 PEEKVAL() =
		   INT_TO_REF (
			       SPATIC_PTR (p) ?
			       p - spatic.start :
			       NEW_PTR (p) ?
			       (p - new_space.start) + spatic.size :
			       (	/* This is one weird reference: */
				printf ("GET-DATA of "),
				printref (stdout, x),
				printf ("\n"),
				-(long) p - 1)
			       );
	       }
	     else
	       PEEKVAL() = (x & ~TAG_MASKL) | INT_TAG;
	     GOTO_TOP;

	   case 11:		/* CRUNCH */
	     POPVAL(x);	/* data */
	     y = PEEKVAL();	/* tag */
	     CHECKTAGS_INT_1 (x, y, 2);
	     {
	       int tag = (REF_TO_INT (y) & TAG_MASK);
	       ref_t z;

	       if (tag & PTR_MASK)
		 {
		   long i = REF_TO_INT (x);

		   /* For now, preclude creation of very odd references. */
		   TRAP1_IF (i < 0, 2);
		   if (i < (long) spatic.size)
		     z = PTR_TO_LOC (spatic.start + i);
		   else if (i < (long) (spatic.size + new_space.size))
		     z = PTR_TO_LOC (new_space.start + (i - spatic.size));
		   else
		     {
		       TRAP1 (2);
		     }
		 }
	       else
		 z = x;

	       PEEKVAL() = z | tag;
	     }
	     GOTO_TOP;

	   case 12:		/* GETC */
	     /* Used in emergency cold load standard-input stream. */
	     PUSHVAL_IMM (CHAR_TO_REF (getc (stdin)));
	     GOTO_TOP;

	   case 13:		/* PUTC */
	     /* Used in emergency cold load standard-output stream and
		for the warm boot message. */
	     x = PEEKVAL();
	     CHECKCHAR0 (x, 1);
	     putc (REF_TO_CHAR (x), stdout);
	     fflush (stdout);
	     if (trace_insts || trace_stcon || trace_cxcon)
	       printf ("\n");
	     GOTO_TOP;

	   case 14:		/* CONTENTS */
	     x = PEEKVAL();
	     CHECKTAG0 (x, LOC_TAG, 1);
	     PEEKVAL() = *LOC_TO_PTR (x);
	     GOTO_TOP;

	   case 15:		/* SET-CONTENTS */
	     POPVAL(x);
	     CHECKTAG1 (x, LOC_TAG, 2);
	     *LOC_TO_PTR (x) = PEEKVAL();
	     GOTO_TOP;

	   case 16:		/* LOAD-TYPE */
	     PEEKVAL() = get_type (PEEKVAL());
	     GOTO_TOP;

	   case 17:		/* CONS */
	     {
	       ref_t *p;

	       ALLOCATE_SS (p, 3, "space crunch in CONS instruction");

	       POPVAL(x);
	       p[CONS_PAIR_CAR_OFF] = x;
	       p[CONS_PAIR_CDR_OFF] = PEEKVAL();
	       p[0] = e_cons_type;
	       PEEKVAL() = PTR_TO_REF (p);

	       GOTO_TOP;
	     }

	   case 18:		/* <0? */
	     x = PEEKVAL();
	     CHECKTAG0 (x, INT_TAG, 1);
	     /* Tag trickery: */

	     if ((long) x < 0)
	       {
		 PEEKVAL() = e_t;
		 GOTO_TOP;
	       }
	     else
	       {
		 PEEKVAL() = e_nil;
		 GOTO_TOP;
	       }


	   case 19:		/* MODULO */
	     /* Sign of divisor (thing being divided by). */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     if (y == INT_TO_REF (0))
	       TRAP1 (2);
	     {
	       long a = REF_TO_INT (x) % REF_TO_INT (y);
	       if ((a < 0 && (long) y > 0) || ((long) y < 0 && (long) x > 0 && a > 0))
		 a += REF_TO_INT (y);
	       PEEKVAL() = INT_TO_REF (a);
	     }
	     GOTO_TOP;

	   case 20:		/* ASH */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     /* Tag trickery: */
	     {
	       long b = REF_TO_INT (y);
	       if (b < 0)
		 {
		   PEEKVAL() = ((long) x >> -b) & ~TAG_MASKL;
		   GOTO_TOP;
		 }
	       else
		 {
		   PEEKVAL() = x << b;
		   GOTO_TOP;
		 }
	     }

	   case 21:		/* ROT */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     /* Rotations can not overflow, but are kind of meaningless in
		the infinite precision model we have.  This instr is used
		only for computing string hashes and stuff like that. */
	     {
	       unsigned long a = (unsigned long) x;
	       long b = REF_TO_INT (y);

	       if (b < 0)
		 {
		   PEEKVAL() = (a >> -b | a << (WORDSIZE - 2 + b)) & ~TAG_MASKL;
		   GOTO_TOP;
		 }
	       else
		 {
		   PEEKVAL() = (a << b | a >> (WORDSIZE - 2 - b)) & ~TAG_MASKL;
		   GOTO_TOP;
		 }
	     }

	   case 22:		/* STORE-BP-I */
	     POPVAL(x);
	     CHECKTAG1 (x, INT_TAG, 2);
	     *(e_bp + REF_TO_INT (x)) = PEEKVAL();
	     GOTO_TOP;

	   case 23:		/* LOAD-BP-I */
	     x = PEEKVAL();
	     CHECKTAG0 (x, INT_TAG, 1);
	     PEEKVAL() = *(e_bp + REF_TO_INT (x));
	     GOTO_TOP;

	   case 24:		/* RETURN */
	     POP_CONTEXT ();
	     GOTO_TOP;

	   case 25:		/* ALLOCATE */
	     {
	       ref_t *p;

	       POPVAL(x);
	       y = PEEKVAL();
	       CHECKTAG1 (y, INT_TAG, 2);

	       ALLOCATE1 (p, REF_TO_INT (y),
			  "space crunch in ALLOCATE instruction", x);

	       *p = x;

	       PEEKVAL() = PTR_TO_REF (p);

	       while (++p < free_point)
		 *p = NEW_STORAGE;
	       GOTO_TOP;
	     }

	   case 26:		/* ASSQ */
	     {
	       ref_t z, t;

	       POPVAL(z);
	       x = PEEKVAL();
	       /* y = assq(z,x); */
	       while (x != e_nil && car (t = car (x)) != z)
		 x = cdr (x);

	       if (x == e_nil)
		 {
		   PEEKVAL() = e_nil;
		   GOTO_TOP;
		 }
	       else
		 {
		   /*
		     PEEKVAL() = car (x);
		   */
		   PEEKVAL() = t;
		   GOTO_TOP;
		 }

	     }
	   case 27:		/* LOAD-LENGTH */
	     x = PEEKVAL();
	     PEEKVAL() =
	       (TAG_IS (x, PTR_TAG) ?
		(REF_SLOT (REF_SLOT (x, 0), TYPE_VAR_LEN_P_OFF) == e_nil ?
		 REF_SLOT (REF_SLOT (x, 0), TYPE_LEN_OFF) :
		 REF_SLOT (x, 1)) :
		INT_TO_REF (0));
	     GOTO_TOP;

	   case 28:		/* PEEK */
	     PEEKVAL() = INT_TO_REF( *(u_int16_t *)PEEKVAL() );
	     GOTO_TOP;

	   case 29:		/* POKE */
	     POPVAL(x);
	     *(u_int16_t *)x = (u_int16_t)REF_TO_INT(PEEKVAL());
	     GOTO_TOP;

	   case 30:		/* MAKE-CELL */
	     {
	       ref_t *p;

	       ALLOCATE_SS (p, 1, "space crunch in MAKE-CELL instruction");

	       *p = PEEKVAL();
	       PEEKVAL() = PTR_TO_LOC (p);
	       GOTO_TOP;
	     }

	   case 31:		/* SUBTRACT */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);

	     {
	       long a = REF_TO_INT (x) - REF_TO_INT (y);
	       OVERFLOWN_INT (a, TRAP1 (2));
	       PEEKVAL() = INT_TO_REF (a);
	       GOTO_TOP;
	     }


	   case 32:		/* = */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     if (x == y)
	       {
		 PEEKVAL() = e_t;
		 GOTO_TOP;
	       }
	     else
	       {
		 PEEKVAL() = e_nil;
		 GOTO_TOP;
	       }


	   case 33:		/* < */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     /* Tag trickery: */
	     if ((long) x < (long) y)
	       {
		 PEEKVAL() = e_t;
		 GOTO_TOP;
	       }
	     else
	       {
		 PEEKVAL() = e_nil;
		 GOTO_TOP;
	       }


	   case 34:		/* LOG-NOT */
	     x = PEEKVAL();
	     CHECKTAG0 (x, INT_TAG, 1);
	     /* Tag trickery: */

	     PEEKVAL() = ~x - (TAG_MASK - INT_TAG);

	     GOTO_TOP;

	   case 35:		/* LONG-BRANCH distance (signed) */
	     POLL_SIGNALS ();
	     local_epc += ASHR2 (SIGN_16BIT_ARG (*local_epc)) + 1;
	     GOTO_TOP;

	   case 36:		/* LONG-BRANCH-NIL distance (signed) */
	     POLL_SIGNALS ();
	     POPVAL(x);
	     if (x != e_nil)
	       {
		 local_epc++;
		 GOTO_TOP;
	       }
	     else
	       {
		 local_epc += ASHR2 (SIGN_16BIT_ARG (*local_epc)) + 1;
		 GOTO_TOP;
	       }



	   case 37:		/* LONG-BRANCH-T distance (signed) */
	     POLL_SIGNALS ();
	     POPVAL(x);
	     if (x == e_nil)
	       {
		 local_epc++;
		 GOTO_TOP;
	       }
	     else
	       {
		 local_epc += ASHR2 (SIGN_16BIT_ARG (*local_epc)) + 1;
		 GOTO_TOP;
	       }


	   case 38:		/* LOCATE-BP-I */
	     x = PEEKVAL();
	     CHECKTAG0 (x, INT_TAG, 1);
	     PEEKVAL() = PTR_TO_LOC (e_bp + REF_TO_INT (x));
	     GOTO_TOP;

	   case 39:		/* LOAD-IMM-CON ; INLINE-REF */
	     /* This is like a LOAD-IMM followed by a CONTENTS. */
	     /* align pc to next word boundary: */

	     /* Do it in ?two? instructions: */
	     /* local_epc = (unsigned short*)(((unsigned long)local_epc + 3)&~3ul); */
	     /* Do it in ?three? instructions including branch: */
	     if ((unsigned long) local_epc & 2)
	       local_epc++;

	     /*NOSTRICT */
	     x = *(ref_t *) local_epc;
	     local_epc += 2;

	     /* This checktag looks buggy, since it's hard to back over
		the instruction normally ... need to expand this out */
	     CHECKTAG1 (x, LOC_TAG, 1);
	     PUSHVAL(*LOC_TO_PTR (x));
	     GOTO_TOP;

	     /* Cons instructions. */

#define CONSINSTR(a, ins)					\
		{ x = PEEKVAL();				\
		  CHECKTAG0(x, PTR_TAG, a);			\
		  if (REF_SLOT(x,0) != e_cons_type) { TRAP0(a); } }

	   case 40:		/* CAR */
	     CONSINSTR (1, CAR);
	     PEEKVAL() = car (x);
	     GOTO_TOP;

	   case 41:		/* CDR */
	     CONSINSTR (1, CDR);
	     PEEKVAL() = cdr (x);
	     GOTO_TOP;

	   case 42:		/* SET-CAR */
	     CONSINSTR (2, SET - CAR);
	     POPVALS (1);
	     car (x) = PEEKVAL();
	     GOTO_TOP;

	   case 43:		/* SET-CDR */
	     CONSINSTR (2, SET - CDR);
	     POPVALS (1);
	     cdr (x) = PEEKVAL();
	     GOTO_TOP;

	   case 44:		/* LOCATE-CAR */
	     CONSINSTR (1, LOCATE - CAR);
	     PEEKVAL() = PTR_TO_LOC (&car (x));
	     GOTO_TOP;

	   case 45:		/* LOCATE-CDR */
	     CONSINSTR (1, LOCATE - CDR);
	     PEEKVAL() = PTR_TO_LOC (&cdr (x));
	     GOTO_TOP;

	     /* Done with cons instructions. */

	   case 46:		/* PUSH-CXT-LONG rel */
	     PUSH_CONTEXT (ASHR2 (SIGN_16BIT_ARG (*local_epc)) + 1);
	     local_epc++;
	     GOTO_TOP;

	   case 47:		/* Call a primitive routine. */
	     fprintf(stderr, "Not configured for CALL-PRIMITIVE.\n");
	     GOTO_TOP;

	   case 48:		/* THROW */
	     POPVAL(x);
	     CHECKTAG1 (x, PTR_TAG, 2);
	     y = PEEKVAL();
	     BASH_VAL_HEIGHT (REF_TO_INT (REF_SLOT (x, ESCAPE_OBJECT_VAL_OFF)));
	     BASH_CXT_HEIGHT (REF_TO_INT (REF_SLOT (x, ESCAPE_OBJECT_CXT_OFF)));
	     PUSHVAL(y);
	     POP_CONTEXT ();
	     GOTO_TOP;

	   case 49:		/* GET-WP */
	     PEEKVAL() = ref_to_wp (PEEKVAL());
	     GOTO_TOP;

	   case 50:		/* WP-CONTENTS */
	     x = PEEKVAL();
	     CHECKTAG0 (x, INT_TAG, 1);
	     PEEKVAL() = wp_to_ref (x);
	     GOTO_TOP;

	   case 51:		/* GC */
	     value_stack.sp = local_value_sp;
	     context_stack.sp = local_context_sp;
	     e_pc = local_epc;
	     gc (false, false, "explicit call", 0);
	     local_value_sp = value_stack.sp;
	     local_context_sp = context_stack.sp;
	     local_epc = e_pc;
	     PUSHVAL(e_nil);
	     GOTO_TOP;

	   case 52:		/* BIG-ENDIAN? */
	     if (byte_gender == big_endian)
	       {
		 PUSHVAL(e_t);
		 GOTO_TOP;
	       }
	     else
	       {
		 PUSHVAL(e_nil);
		 GOTO_TOP;
	       }


	   case 53:		/* VLEN-ALLOCATE */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAG1 (y, INT_TAG, 2);
	     {
	       ref_t *p;

	       ALLOCATE1 (p, REF_TO_INT (y),
			  "space crunch in VARLEN-ALLOCATE instruction", x);

	       PEEKVAL() = PTR_TO_REF (p);

	       p[0] = x;
	       p[1] = y;
	       p += 2;

	       while (p < free_point)
		 *p++ = NEW_STORAGE;
	     }
	     GOTO_TOP;

	   case 54:		/* INC-LOC */
	     /* Increment a locative by an amount.  This is an instruction
		rather than (%crunch (+ (%pointer loc) index) %locative-tag)
		to avoid a window of gc vulnerability.  All such windows
		must be fully closed before engines come up. */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS1 (x, LOC_TAG, y, INT_TAG, 2);
	     PEEKVAL() = PTR_TO_LOC (LOC_TO_PTR (x) + REF_TO_INT (y));
	     GOTO_TOP;

	   case 55:		/* FILL-CONTINUATION */
	     /* This instruction fills a continuation object with
		the appropriate values. */
	     CHECKVAL_POP (1);
	     VALUE_FLUSH (2);
	     CONTEXT_FLUSH (0);
#ifndef FAST
	     /* debugging check: */
	     if (local_value_sp != &value_stack_bp[1])
	       printf ("Value stack flushing error.\n");
	     if (local_context_sp != &context_stack_bp[-1])
	       printf ("Context stack flushing error.\n");
#endif
	     x = PEEKVAL();
	     /* CHECKTAG0(x,PTR_TAG,1); */
	     REF_SLOT (x, CONTINUATION_VAL_SEGS)
	       = value_stack.segment;
	     REF_SLOT (x, CONTINUATION_VAL_OFF)
	       = INT_TO_REF (value_stack.pushed_count);
	     REF_SLOT (x, CONTINUATION_CXT_SEGS)
	       = context_stack.segment;
	     REF_SLOT (x, CONTINUATION_CXT_OFF)
	       = INT_TO_REF (context_stack.pushed_count);
	     /* Maybe it's a good idea to reload the buffer, but I'm
		not bothering and things seem to work. */
	     /* CHECKCXT_POP(0); */
	     GOTO_TOP;

	   case 56:		/* CONTINUE */
	     /* Continue a continuation. */
	     /* Grab the continuation. */

	     POPVAL(x);
	     /* CHECKTAG1(x,PTR_TAG,1); */
	     y = PEEKVAL();
	     /* Pull the crap out of it. */

	     value_stack.segment
	       = REF_SLOT (x, CONTINUATION_VAL_SEGS);
	     value_stack.pushed_count
	       = REF_TO_INT (REF_SLOT (x, CONTINUATION_VAL_OFF));

	     local_value_sp = &value_stack_bp[-1];
	     PUSHVAL_NOCHECK (y);

	     context_stack.segment
	       = REF_SLOT (x, CONTINUATION_CXT_SEGS);
	     context_stack.pushed_count
	       = REF_TO_INT (REF_SLOT (x, CONTINUATION_CXT_OFF));
	     local_context_sp = &context_stack_bp[-1];
	     POP_CONTEXT ();
	     GOTO_TOP;

	   case 57:		/* REVERSE-CONS */
	     /* This is just like CONS except that it takes its args
		in the other order.  Makes open coded LIST better. */

	     {
	       ref_t *p;

	       ALLOCATE_SS (p, 3, "space crunch in REVERSE-CONS instruction");

	       POPVAL(x);
	       p[CONS_PAIR_CDR_OFF] = x;
	       p[CONS_PAIR_CAR_OFF] = PEEKVAL();
	       p[0] = e_cons_type;
	       PEEKVAL() = PTR_TO_REF (p);

	       GOTO_TOP;
	     }


	   case 58:		/* MOST-NEGATIVE-FIXNUM? */

	     if (PEEKVAL() == MIN_REF)
	       {
		 PEEKVAL() = e_t;
		 GOTO_TOP;
	       }
	     else
	       {
		 PEEKVAL() = e_nil;
		 GOTO_TOP;
	       }


	   case 59:		/* FX-PLUS */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     /* Tag trickery: */
	     PEEKVAL() = x + y;
	     GOTO_TOP;

	   case 60:		/* FX-TIMES */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     /* Tag trickery: */
	     PEEKVAL() = REF_TO_INT (x) * y;
	     GOTO_TOP;

	   case 61:		/* GET-TIME */
	     /* Return CPU time */
	     PUSHVAL_IMM (INT_TO_REF (get_user_time ()));
	     GOTO_TOP;

	   case 62:		/* REMAINDER */
	     /* Sign of dividend (thing being divided.) */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     if (y == INT_TO_REF (0))
	       TRAP1 (2);
	     PEEKVAL() = INT_TO_REF (REF_TO_INT (x) % REF_TO_INT (y));
	     GOTO_TOP;

	   case 63:		/* QUOTIENTM */
	     /* Round towards -inf.  Obeys identity w/ MODULO. */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     /* Can't divide by 0, or the most negative fixnum by -1. */
	     if (y == INT_TO_REF (0) ||
		 (y == INT_TO_REF (-1) && x == MIN_REF))
	       TRAP1 (2);
	     /* Tag trickery: */
	     /* I can't seem to get anything like this to work: */

	     PEEKVAL() = INT_TO_REF ((((long) x < 0) ^ ((long) y < 0))
				      ? -(long) x / -(long) y
				      : (long) x / (long) y);

	     {
	       long a = (long) x / (long) y;
	       if (((long) x < 0 && (long) y > 0 && a * (long) y > (long) x) ||
		   ((long) y < 0 && (long) x > 0 && a * (long) y < (long) x))
		 a -= 1;
	       PEEKVAL() = INT_TO_REF (a);
	     }
	     GOTO_TOP;

	   case 64:		/* FULL-GC */
	     value_stack.sp = local_value_sp;
	     context_stack.sp = local_context_sp;
	     e_pc = local_epc;

	     gc (false, true, "explicit call", 0);

	     local_value_sp = value_stack.sp;
	     local_context_sp = context_stack.sp;
	     local_epc = e_pc;
	     PUSHVAL(e_nil);
	     GOTO_TOP;

	   case 65:		/* MAKE-LAMBDA */
	     {
	       ref_t *p;

	       ALLOCATE_SS (p, 8, "space crunch in MAKE-LAMBDA instruction");


	       p[0] = e_operation_type;
	       p[OPERATION_LAMBDA_OFF] = PTR_TO_REF (p + OPERATION_LENGTH);
	       p[OPERATION_CACHE_TYPE_OFF] = NEW_STORAGE;
	       p[OPERATION_CACHE_METH_OFF] = NEW_STORAGE;
	       p[OPERATION_CACHE_TYPE_OFF_OFF] = NEW_STORAGE;

	       POPVAL(x);
	       p[OPERATION_LENGTH + METHOD_CODE_OFF] = x;
	       p[OPERATION_LENGTH + METHOD_ENV_OFF] = PEEKVAL();
	       p[OPERATION_LENGTH] = e_method_type;
	       PEEKVAL() = PTR_TO_REF (p);
	       GOTO_TOP;
	     }

	   case 66:		/* GET-ARGLINE-CHAR */
	     /* takes two args on stack, index into argv and index into
		that argument.  Return a character (perhaps nul), or
		#f if out of bounds */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     {
	       int c = program_arg_char(REF_TO_INT(x), REF_TO_INT(y));
	       PEEKVAL() = (c == -1) ? e_nil : CHAR_TO_REF(c);
	     }
	     GOTO_TOP;

#ifndef FAST
	   default:
	     printf ("\nError (vm interpreter): "
		     "Illegal parameterless instruction %d.\n", arg_field);
	     value_stack.sp = local_value_sp;
	     context_stack.sp = local_context_sp;
	     e_pc = local_epc;
	     maybe_dump_world (333);
	     exit (EXIT_FAILURE);
#endif
	   }

       }
     else
       {			/* parametric instructions */

	 switch (op_field)
	   {
#ifndef FAST
	   case (0):		/*  PARAMETERLESS-INSTRUCTION xxxx */
	     fprintf (stderr,
		      "Error (vm interpreter): Internal error "
		      "file: %s line: %d\n", __FILE__, __LINE__);
	     exit (EXIT_FAILURE);
#endif
	   case (1):		/* HALT n */
	     {
	       int halt_code = arg_field;

	       value_stack.sp = local_value_sp;
	       context_stack.sp = local_context_sp;
	       e_pc = local_epc;
	       maybe_dump_world (halt_code);
	       exit (halt_code);
	     }

	   case (2):		/* LOG-OP log-spec */
	     POPVAL(x);
	     y = PEEKVAL();
	     CHECKTAGS_INT_1 (x, y, 2);
	     /* Tag trickery: */
	     PEEKVAL() = ((instr & (1 << 8) ? x & y : 0)
			  |(instr & (1 << 9) ? ~x & y : 0)
			  |(instr & (1 << 10) ? x & ~y : 0)
			  |(instr & (1 << 11) ? ~x & ~y : 0)) & ~TAG_MASKL;
	     GOTO_TOP;

	   case (3):		/* BLT-STACK stuff,trash */
	     {
	       unsigned int stuff = arg_field & 0xf;
	       unsigned int trash_m1 = arg_field >> 4;

	       CHECKVAL_POP (stuff + trash_m1);

	       {
		 ref_t *src = local_value_sp - stuff;
		 ref_t *dest = src - (trash_m1 + 1);

		 while (src < local_value_sp)
		   *++dest = *++src;

		 local_value_sp = dest;
	       }
	     }
	     GOTO_TOP;


	   case (4):		/* BRANCH-NIL distance (signed) */

	     POLL_SIGNALS ();


	     POPVAL(x);
	     if (x == e_nil)
	       {
		 local_epc += signed_arg_field;
	       }
	     GOTO_TOP;



	   case (5):		/* BRANCH-T distance (signed) */

	     POLL_SIGNALS ();

	     POPVAL(x);
	     if (x != e_nil)
	       {
		 local_epc += signed_arg_field;
	       }
	     GOTO_TOP;


	   case (6):		/* BRANCH distance (signed) */


	     POLL_SIGNALS ();

	     local_epc += signed_arg_field;
	     GOTO_TOP;


	   case (7):		/* POP n */
	     POPVALS ((int) arg_field);
	     GOTO_TOP;

	   case (8):		/* SWAP n */
	     {
	       ref_t *other;
	       MAKE_BACK_VAL_PTR (other, (int) arg_field);
	       x = PEEKVAL();
	       PEEKVAL() = *other;
	       *other = x;
	     }
	     GOTO_TOP;

	   case (9):		/* BLAST n */
	     CHECKVAL_POP ((int) arg_field);
	     {
	       ref_t *other = local_value_sp - arg_field;
	       *other = POPVAL_NOCHECK ();
	     }
	     GOTO_TOP;

	   case (10):		/* LOAD-IMM-FIX signed-arg */
	     /* Tag trickery and opcode knowledge changes this
		PUSHVAL_IMM(INT_TO_REF(signed_arg_field));
		to this: */
	     PUSHVAL_IMM( (ref_t)(((int16_t)instr) >> 6) );
	     GOTO_TOP;

	   case (11):		/* STORE-STK n */
	     {
	       ref_t *other;

	       MAKE_BACK_VAL_PTR (other, (int)arg_field);
	       *other = PEEKVAL();
	     }
	     GOTO_TOP;


	   case (12):		/* LOAD-BP n */
	     PUSHVAL(*(e_bp + arg_field));
	     GOTO_TOP;

	   case (13):		/* STORE-BP n */
	     *(e_bp + arg_field) = PEEKVAL();
	     GOTO_TOP;

	   case (14):		/* LOAD-ENV n */
	     PUSHVAL(*(e_env + arg_field));
	     GOTO_TOP;

	   case (15):		/* STORE-ENV n */
	     *(e_env + arg_field) = PEEKVAL();
	     GOTO_TOP;

	   case (16):		/* LOAD-STK n */
	     /* All attempts to start this with if (arg_field == 0)
		for speed have failed, so benchmark carefully before 
		trying it. */
	     {
	       ref_t *other;
	       MAKE_BACK_VAL_PTR (other, (int) arg_field);
	       PUSHVAL(*other);
	       GOTO_TOP;
	     }


	   case (17):		/* MAKE-BP-LOC n */
	     PUSHVAL(PTR_TO_LOC (e_bp + arg_field));
	     GOTO_TOP;

	   case (18):		/* MAKE-ENV-LOC n */
	     PUSHVAL(PTR_TO_LOC (e_env + arg_field));
	     GOTO_TOP;

	   case (19):		/* STORE-REG reg */
	     x = PEEKVAL();
	     switch (arg_field)
	       {
	       case 0:
		 e_t = x;
		 GOTO_TOP;
	       case 1:
		 e_nil = x;
		 wp_table[0] = e_nil;
		 rebuild_wp_hashtable ();
		 GOTO_TOP;
	       case 2:
		 e_fixnum_type = x;
		 GOTO_TOP;
	       case 3:
		 e_loc_type = x;
		 GOTO_TOP;
	       case 4:
		 e_cons_type = x;
		 GOTO_TOP;
	       case 5:
		 CHECKTAG1 (x, PTR_TAG, 1);
		 e_subtype_table = REF_TO_PTR (x) + 2;
		 GOTO_TOP;
	       case 6:
		 CHECKTAG1 (x, LOC_TAG, 1);
		 e_bp = LOC_TO_PTR (x);
		 GOTO_TOP;
	       case 7:
		 CHECKTAG1 (x, PTR_TAG, 1);
		 e_env = REF_TO_PTR (x);
		 GOTO_TOP;
	       case 8:
		 CHECKTAG1 (x, INT_TAG, 1);
		 e_nargs = REF_TO_INT (x);
		 GOTO_TOP;
	       case 9:
		 e_env_type = x;
		 GOTO_TOP;
	       case 10:
		 CHECKTAG1 (x, PTR_TAG, 1);
		 e_argless_tag_trap_table = REF_TO_PTR (x) + 2;
		 GOTO_TOP;
	       case 11:
		 CHECKTAG1 (x, PTR_TAG, 1);
		 e_arged_tag_trap_table = REF_TO_PTR (x) + 2;
		 GOTO_TOP;
	       case 12:
		 e_object_type = x;
		 GOTO_TOP;
	       case 13:
		 e_boot_code = x;
		 GOTO_TOP;
	       case 14:
		 CHECKTAG1 (x, LOC_TAG, 1);
		 free_point = LOC_TO_PTR (x);
		 GOTO_TOP;
	       case 15:
		 CHECKTAG1 (x, LOC_TAG, 1);
		 new_space.end = LOC_TO_PTR (x);
		 GOTO_TOP;
	       case 16:
		 e_segment_type = x;
		 BASH_SEGMENT_TYPE ();
		 GOTO_TOP;
	       case 17:
		 e_uninitialized = x;
		 GOTO_TOP;
	       case 18:
		 CHECKTAG1 (x, INT_TAG, 1);
		 e_next_newspace_size = REF_TO_INT (x);
#ifdef MAX_NEW_SPACE_SIZE
		 if (e_next_newspace_size > MAX_NEW_SPACE_SIZE)
		   e_next_newspace_size = MAX_NEW_SPACE_SIZE;
#endif
		 GOTO_TOP;
	       case 19:
		 e_method_type = x;
		 GOTO_TOP;
	       case 20:
		 e_operation_type = x;		/*qqq */
		 GOTO_TOP;
	       default:
		 printf ("STORE-REG %d, unknown .\n", arg_field);
		 GOTO_TOP;
	       }
#ifdef _ICC
	     break;
#endif

	   case (20):		/* LOAD-REG reg */
	     switch (arg_field)
	       {
	       case 0:
		 PUSHVAL(e_t);
		 GOTO_TOP;
	       case 1:
		 PUSHVAL(e_nil);
		 GOTO_TOP;
	       case 2:
		 PUSHVAL(e_fixnum_type);
		 GOTO_TOP;
	       case 3:
		 PUSHVAL(e_loc_type);
		 GOTO_TOP;
	       case 4:
		 PUSHVAL(e_cons_type);
		 GOTO_TOP;
	       case 5:
		 PUSHVAL(PTR_TO_REF (e_subtype_table - 2));
		 GOTO_TOP;
	       case 6:
		 PUSHVAL(PTR_TO_LOC (e_bp))
		   GOTO_TOP;
	       case 7:
		 PUSHVAL(PTR_TO_REF (e_env));
		 GOTO_TOP;
	       case 8:
		 PUSHVAL(INT_TO_REF ((long) e_nargs));
		 GOTO_TOP;
	       case 9:
		 PUSHVAL(e_env_type);
		 GOTO_TOP;
	       case 10:
		 PUSHVAL(PTR_TO_REF (e_argless_tag_trap_table - 2));
		 GOTO_TOP;
	       case 11:
		 PUSHVAL(PTR_TO_REF (e_arged_tag_trap_table - 2));
		 GOTO_TOP;
	       case 12:
		 PUSHVAL(e_object_type);
		 GOTO_TOP;
	       case 13:
		 PUSHVAL(e_boot_code);
		 GOTO_TOP;
	       case 14:
		 PUSHVAL(PTR_TO_LOC (free_point));
		 GOTO_TOP;
	       case 15:
		 PUSHVAL(PTR_TO_LOC (new_space.end));
		 GOTO_TOP;
	       case 16:
		 PUSHVAL(e_segment_type);
		 GOTO_TOP;
	       case 17:
		 PUSHVAL(e_uninitialized);
		 GOTO_TOP;
	       case 18:
		 PUSHVAL(INT_TO_REF (e_next_newspace_size));
		 GOTO_TOP;
	       case 19:
		 PUSHVAL(e_method_type);
		 GOTO_TOP;
	       case 20:
		 PUSHVAL(e_operation_type);
		 GOTO_TOP;
	       default:
		 fprintf (stderr, "Error (vm interpreter): "
			  "LOAD-REG %d, unknown .\n", arg_field);
		 PUSHVAL(e_nil);
		 GOTO_TOP;
	       }
#ifdef _ICC
	     break;
#endif


	   case (21):		/* FUNCALL-CXT, FUNCALL-CXT-BR distance (signed) */


	     POLL_SIGNALS ();

	     /* NOTE: (FUNCALL-CXT) == (FUNCALL-CXT-BR 0) */
	     PUSH_CONTEXT (signed_arg_field);

	     /* Fall through to tail recursive case: */
	     goto funcall_tail;


	   case (22):		/* FUNCALL-TAIL */

	     /* This polling should not be moved below the trap label, 
		since the interrupt code will fail on a fake 
		instruction failure. */

	     POLL_SIGNALS ();


	     /* This label allows us to branch back up from a trap. */

	     /***********/
	   funcall_tail:
	     /***********/

	     x = PEEKVAL();
	     CHECKTAG0 (x, PTR_TAG, e_nargs + 1);
	     CHECKVAL_POP (1);
	     y = PEEKVAL_UP (1);

	     e_current_method = REF_SLOT (x, OPERATION_LAMBDA_OFF);

	     if (e_current_method == e_nil)
	       {		/* SEARCH */
		 ref_t y_type = (e_nargs == 0) ? e_object_type : get_type (y);

#ifndef NO_METH_CACHE
		 /* Check for cache hit: */
		 if (y_type == REF_SLOT (x, OPERATION_CACHE_TYPE_OFF))
		   {
		     MAYBE_PUT (trace_mcache, "H");
		     e_current_method = REF_SLOT (x, OPERATION_CACHE_METH_OFF);
		     e_bp =
		       REF_TO_PTR (y) +
		       REF_TO_INT (REF_SLOT (x, OPERATION_CACHE_TYPE_OFF_OFF));
		   }
		 else
#endif
		   {
		     /* Search the type hierarchy. */
		     ref_t meth_type, offset = INT_TO_REF (0);

		     /******************************************************
									    find_method_type_pair(x, y_type,
									    &e_current_method, &meth_type);
		     */

		     {
		       ref_t obj_type = y_type;
		       ref_t alist;
		       ref_t *locl = 0;
		       ref_t thelist;
		       ref_t *loclist;
		       ref_t *llp = &later_lists[-1];

		       while (1)	/* forever */
			 {
			   /* First look for it in the local method alist of obj_type: */

			   alist = thelist =
			     *(loclist = &REF_SLOT (obj_type, TYPE_OP_METHOD_ALIST_OFF));

			   while (alist != e_nil)
			     {
			       if (car (car (alist)) == x)
				 {
				   if (locl != 0)
				     {
				       *locl = cdr (alist);
				       *loclist = alist;
				       cdr (alist) = thelist;
				     }
				   e_current_method = cdr (car (alist));
				   meth_type = obj_type;
				   goto found_it;
				 }
			       alist = *(locl = &cdr (alist));
			     }

			   /* Not there, stack the supertype list and then fetch the top guy
			      available from the stack. */

			   *++llp = REF_SLOT (obj_type, TYPE_SUPER_LIST_OFF);

			   while (*llp == e_nil)
			     {
			       if (llp == later_lists)
				 {
				   if (trace_traps)
				     (void) printf ("No handler for operation!\n");
				   TRAP0(e_nargs + 1);
				 }
			       llp -= 1;
			     }

			   locl = 0;
			   obj_type = car (*llp);
			   *llp = cdr (*llp);
			 }
		     }

		   found_it:


		     /******************************************************/

		     /*
		       if (e_current_method == e_nil)
		       {
		       if (trace_traps)
		       (void)printf("No handler for operation!\n");
		       TRAP1(e_nargs+1);
		       }
		     */

		     /* This could be dispensed with if meth_type has no
			ivars and isn't variable-length-mixin. */
		     {
		       ref_t alist
			 = REF_SLOT (y_type, TYPE_TYPE_BP_ALIST_OFF);

		       while (alist != e_nil)
			 {
			   if (car (car (alist)) == meth_type)
			     {
			       offset = cdr (car (alist));
			       break;
			     }
			   alist = cdr (alist);
			 }
		     }
		     e_bp = REF_TO_PTR (y) + REF_TO_INT (offset);

#ifndef NO_METH_CACHE
		     MAYBE_PUT (trace_mcache, "M");
		     /* Cache the results of this search. */
		     REF_SLOT (x, OPERATION_CACHE_TYPE_OFF) = y_type;
		     REF_SLOT (x, OPERATION_CACHE_METH_OFF) = e_current_method;
		     REF_SLOT (x, OPERATION_CACHE_TYPE_OFF_OFF) = offset;
#endif
		   }
	       }
	     else if (!TAG_IS (e_current_method, PTR_TAG)
		      || REF_SLOT (e_current_method, 0) != e_method_type)
	       {
		 /* TAG TRAP */
		 if (trace_traps)
		   printf ("Bogus or never defined operation.\n");
		 TRAP0(e_nargs + 1);
	       }
	     /* else it's a LAMBDA. */

	     x = e_current_method;

	     e_env = REF_TO_PTR (REF_SLOT (x, METHOD_ENV_OFF));
	     local_epc = CODE_SEG_FIRST_INSTR (e_code_segment =
					       REF_SLOT (x, METHOD_CODE_OFF));
	     GOTO_TOP;

	   case (23):		/* STORE-NARGS n */
	     e_nargs = arg_field;
	     GOTO_TOP;

	   case (24):		/* CHECK-NARGS n */
	     if (e_nargs == arg_field)
	       {
		 POPVALS (1);
		 GOTO_TOP;
	       }
	     else
	       {
		 if (trace_traps)
		   printf ("\n%d args passed; %d expected.\n",
			   e_nargs, arg_field);
		 TRAP0(e_nargs + 1);
	       }

	   case (25):		/* CHECK-NARGS-GTE n */
	     if (e_nargs >= arg_field)
	       {
		 POPVALS (1);
		 GOTO_TOP;
	       }
	     else
	       {
		 if (trace_traps)
		   printf ("\n%d args passed; %d or more expected.\n", e_nargs, arg_field);
		 TRAP0(e_nargs + 1);
	       }

	   case (26):		/* STORE-SLOT n */
	     POPVAL(x);
	     CHECKTAG1 (x, PTR_TAG, 2);
	     REF_SLOT (x, arg_field) = PEEKVAL();
	     GOTO_TOP;

	   case (27):		/* LOAD-SLOT n */
	     CHECKTAG0 (PEEKVAL(), PTR_TAG, 1);
	     PEEKVAL() = REF_SLOT (PEEKVAL(), arg_field);
	     GOTO_TOP;

	   case (28):		/* MAKE-CLOSED-ENVIRONMENT n */
	     /* This code might be in error if arg_field == 0, which the
		compiler should never generate. */
	     {
	       ref_t *p;
	       ref_t z;

#ifndef FAST
	       if (arg_field == 0)
		 {
		   fprintf (stderr, "MAKE-CLOSED-ENVIRONMENT 0.\n");
		   fflush (stderr);
		 }
#endif

	       ALLOCATE_SS (p, (long) (arg_field + 2),
			    "space crunch in MAKE-CLOSED-ENVIRONMENT");

	       z = PTR_TO_REF (p);
	       CHECKVAL_POP ((int) arg_field - 1);


	       p[0] = e_env_type;
	       p[1] = INT_TO_REF (arg_field + 2);
	       p += 2;

	       while (arg_field--)
		 *p++ = POPVAL_NOCHECK ();

	       PUSHVAL_NOCHECK (z);

	       GOTO_TOP;
	     }

	   case (29):		/* PUSH-CXT rel */

	     PUSH_CONTEXT (signed_arg_field);
	     GOTO_TOP;


	   case (30):		/* LOCATE-SLOT n */
	     PEEKVAL()
	       = PTR_TO_LOC (REF_TO_PTR (PEEKVAL()) + arg_field);
	     GOTO_TOP;

	   case (31):		/* STREAM-PRIMITIVE n */
	     switch (arg_field)
	       {
	       case 0:	/* n=0: get standard input stream. */
		 PUSHVAL((ref_t) stdin);
		 GOTO_TOP;

	       case 1:	/* n=1: get standard output stream. */
		 PUSHVAL((ref_t) stdout);
		 GOTO_TOP;

	       case 2:	/* n=2: get standard error output stream. */
		 PUSHVAL((ref_t) stderr);
		 GOTO_TOP;

	       case 3:	/* n=3: fopen, mode READ */
	       case 4:	/* n=4: fopen, mode WRITE */
	       case 5:	/* n=5: fopen, mode APPEND */
		 POPVAL(x);
		 /* How about a CHECKTAG(x,LOC_TAG,) here, eh? */
		 {
		   char *s = (char *)oak_c_string((ref_t *) LOC_TO_PTR (x),
						  REF_TO_INT (PEEKVAL()));

		   FILE *fd;
		   if (trace_files) printf ("About to open '%s'.\n", s);
		   fd = fopen(s,
			      arg_field == 3 ? READ_MODE :
			      arg_field == 4 ? WRITE_MODE : APPEND_MODE);
		   free(s);
		   PEEKVAL() = ( (fd == NULL) ? e_nil : (ref_t)fd );
		 }
		 GOTO_TOP;

	       case 6:	/* n=6: fclose */
		 PEEKVAL() =
		   fclose((FILE *) PEEKVAL()) == EOF ? e_nil : e_t;
		 GOTO_TOP;

	       case 7:	/* n=7: fflush */
		 PEEKVAL() =
		   fflush((FILE *) PEEKVAL()) == EOF ? e_nil : e_t;
		 GOTO_TOP;

	       case 8:	/* n=8: putc */
		 POPVAL(x);
		 y = PEEKVAL();
		 CHECKCHAR1 (y, 2);
		 PEEKVAL()
		   = putc (REF_TO_CHAR (y), (FILE *) x) == EOF ? e_nil : e_t;
		 GOTO_TOP;

	       case 9:	/* n=9: getc */
		 {
		   int c = getc ((FILE *) PEEKVAL());
		   /* When possible, if an EOF is read from an interactive
		      stream, the eof should be cleared so regular stuff
		      can be read thereafter. */
		   if (c == EOF)
		     {
		       if (ISATTY ((FILE *) PEEKVAL()))
			 {
			   if (trace_files)
			     printf ("Clearing EOF.\n");

			   clearerr ((FILE *) PEEKVAL());
			 }
		       PEEKVAL() = e_nil;
		     }
		   else
		     PEEKVAL() = CHAR_TO_REF (c);
		 }
		 GOTO_TOP;

	       case 10:	/* n=10: check for interactiveness */
		 PEEKVAL() = ISATTY ((FILE *) PEEKVAL())? e_t : e_nil;
		 GOTO_TOP;

	       case 11:	/* n=11: tell where we are */
		 PEEKVAL() = INT_TO_REF (ftell ((FILE *) PEEKVAL()));
		 GOTO_TOP;

	       case 12:	/* n=12: set where we are */
		 POPVAL(x);
		 {
		   FILE *fd = (FILE *) x;
		   long i = REF_TO_INT (PEEKVAL());

		   PEEKVAL() = fseek (fd, i, 0) == 0 ? e_t : e_nil;
		 }
		 GOTO_TOP;

	       case 13:	/* n=13: change working directory */
		 POPVAL(x);
		 {
		   char *s = oak_c_string((ref_t *)LOC_TO_PTR(x),
					  REF_TO_INT(PEEKVAL()));

		   PEEKVAL() = chdir (s) == 0 ? e_t : e_nil;
		   free (s);
		 }
		 /* if there is no chdir() then use this: */
		 /* PEEKVAL() = e_nil; */
		 GOTO_TOP;

	       default:
		 printf ("\nError (vm interpreter): "
			 "bad stream primitive %d.\n",
			 arg_field);
		 value_stack.sp = local_value_sp;
		 context_stack.sp = local_context_sp;
		 e_pc = local_epc;
		 maybe_dump_world (333);
		 exit (EXIT_FAILURE);
		 GOTO_TOP;
	       }
#ifdef _ICC
	     break;
#endif

	   case (32):		/* FILLTAG n */
	     x = PEEKVAL();
	     CHECKTAG0 (x, PTR_TAG, 1);
	     REF_SLOT (x, ESCAPE_OBJECT_VAL_OFF)
	       = INT_TO_REF (VALUE_STACK_HEIGHT ()
			     - arg_field);
	     REF_SLOT (x, ESCAPE_OBJECT_CXT_OFF)
	       = INT_TO_REF (CONTEXT_STACK_HEIGHT ());
	     GOTO_TOP;

	   case (33):		/* ^SUPER-CXT, ^SUPER-CXT-BR distance */
	     /* Analogous to FUNCALL-CXT[-BR]. */


	     POLL_SIGNALS ();

	     PUSH_CONTEXT (signed_arg_field);

	     /* Fall through to tail recursive case: */
	     goto super_tail;


	   case (34):		/* ^SUPER-TAIL */

	     /* Do not move this below the label! */

	     POLL_SIGNALS ();

	     /******************/
	   super_tail:
	     /******************/
	     /* No cache, no LAMBDA hack, things are easy.
		Maybe not looking at the lambda hack is a bug?

		On stack: type, operation, self, args... */
	     {
	       ref_t the_type;
	       ref_t y_type;
	       ref_t meth_type;

	       POPVAL(the_type);
	       CHECKTAG1 (the_type, PTR_TAG, e_nargs + 2);

	       x = PEEKVAL();	/* The operation. */
	       CHECKTAG1 (x, PTR_TAG, e_nargs + 2);

	       CHECKVAL_POP (1);

	       y = PEEKVAL_UP (1);	/* Self. */

	       y_type = get_type (y);

	       e_current_method = e_nil;

	       find_method_type_pair (x, the_type,
				      &e_current_method, &meth_type);

	       if (e_current_method == e_nil)
		 {
		   if (trace_traps)
		     printf ("No handler for ^super operation.\n");
		   PUSHVAL(the_type);
		   TRAP0(e_nargs + 2);
		 }
	       /* This could be dispensed with if meth_type has no
		  ivars and isn't variable-length-mixin. */
	       {
		 ref_t alist = REF_SLOT (y_type, TYPE_TYPE_BP_ALIST_OFF);
		 ref_t offset = INT_TO_REF (0);

		 while (alist != e_nil)
		   {
		     if (car (car (alist)) == meth_type)
		       {
			 offset = cdr (car (alist));
			 break;
		       }
		     alist = cdr (alist);
		   }
		 e_bp = REF_TO_PTR (y) + REF_TO_INT (offset);
	       }
	     }

	     x = e_current_method;

	     e_env = REF_TO_PTR (REF_SLOT (x, METHOD_ENV_OFF));
	     local_epc = CODE_SEG_FIRST_INSTR (e_code_segment =
					       REF_SLOT (x, METHOD_CODE_OFF));
	     GOTO_TOP;

#ifndef FAST
	   default:
	     printf ("\nError (vm interpreter): "
		     "Illegal parametric instruction % d \n", op_field);
	     value_stack.sp = local_value_sp;
	     context_stack.sp = local_context_sp;
	     e_pc = local_epc;
	     maybe_dump_world (333);
	     exit (EXIT_FAILURE);
#endif
	   }
       }
   }

#ifndef _ICC
  /* The above loop is infinite; we branch down to here when instructions
     fail, normally from tag traps, and then branch back. */
/*************/
intr_trap:
/*************/

  /* clear signal */
/*signal_poll_flag = 0;*/
  if (signal_poll_flag) {
    /* We notify Oaklisp of the user trap by telling it that a noop
       instruction failed.  The Oaklisp trap code must be careful to
       return nothing extra on the stack, and to restore NARGS
       properly.  It is passed the old NARGS. */

    /* the NOOP instruction. */
      arg_field = op_field = instr = 0;
      signal_poll_flag = 0;
#if ENABLE_TIMER
  } else if (timer_counter > TIMEOUT) {
    /* We notify Oaklisp of a timeout trap by telling it that an
       "alarm" instruction failed.  This instruction, bound to
       arg_field 127, does not really exist.  There is, however,
       a handler function bound to that trap. */
      arg_field = 127;
      op_field = 0;
      instr = (127 << 8);
      timer_counter = 0;
#endif
  } else {
    /* How did we get here?  Just do a user trap to get to the debugger. */
      arg_field = op_field = instr = 0;
  }


#ifndef FAST
  if (trace_traps)
    printf ("\nINTR: opcode %d, argfield %d.",
	    op_field, arg_field);

#endif
  /* Back off of the current intruction so it will get executed 
     when we get back from the trap code. */
  local_epc--;

  /* Pass the trap code the current NARGS. */
  x = INT_TO_REF (e_nargs);
  trap_nargs = 1;

#endif

/**************/
arg1_tt:
/**************/

  CHECKVAL_PUSH (3);
  PUSHVAL_NOCHECK (x);

/*************/
arg0_tt:
/*************/

#ifndef FAST
  if (trace_traps)
    {
      printf ("\nTag trap: opcode %d, argfield %d.\n",
	      op_field, arg_field);
      printf ("Top of stack: ");
      printref (stdout, x);
      printf (", pc =  %d\n",
	      ( /* NOSTRICT */
	       SPATIC_PTR ((ref_t *) local_epc)
	       ? ( local_epc - (u_int16_t *) spatic.start)
	       : ( local_epc - (u_int16_t *) new_space.start
		   + 2 * spatic.size )));
    }
#endif
  /* Trick: to preserve tail recursiveness, push context only if next
     instruction isn't a RETURN and current instruction wasn't a FUNCALL.
     or a CHECK-NARGS[-GTE]. */


  if ((op_field < 20 || op_field > 26 || op_field == 23)
      && local_epc[0] != (24 << 8))
    PUSH_CONTEXT (0);

  /* Trapping instructions stash their argument counts here: */
  /* see below */

  if (!op_field)
    {
      /* argless instruction. */
      PUSHVAL_NOCHECK (*(e_argless_tag_trap_table + arg_field));
      e_nargs = trap_nargs;
      /* Set the instruction dispatch  in case the FUNCALL fails. */
      instr = (22 << 2);
      op_field = 22;
      arg_field = e_nargs;
      goto funcall_tail;
    }
  else
    {
      /* arg'ed instruction, so push arg field as extra argument */

      PUSHVAL_NOCHECK (INT_TO_REF (arg_field));
      PUSHVAL_NOCHECK (*(e_arged_tag_trap_table + op_field));
      e_nargs = trap_nargs + 1;
      /* Set the instruction dispatch  in case the FUNCALL fails. */
      instr = (22 << 2);
      op_field = 22;
      arg_field = e_nargs;
      goto funcall_tail;
    }
}
