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


/* The interface between the emulator and natively compiled Oaklisp.

   The native compiler (native-compiler.oak) turns a code block into a
   C function, built into a shared object that the emulator loads with
   dlopen.  The block's code vector stays in the world as a stub, laid
   out as assembled bytecode:

     NATIVE 0, NATIVE 1, ... NATIVE R-1	one per resume point
     I1 NATIVE k1, I2 NATIVE k2, ...	escapes: instructions the C
					hands back to the emulator
     L1, L2, ...			operands: instructions read,
					never run (LOAD-BP etc., whose
					offsets remap-your-ivars rewrites)
     LOAD-IMM c1, LOAD-IMM c2, ...	constants as inline refs, never
					run but resolved by the loader
					and traced by the GC
     LOAD-IMM name, idx, epoch		the C function's name, and the
					index and epoch under which the
					emulator found it

   NATIVE k calls the function with resume point k.  Entry is resume
   point 0; a non-tail call pushes a context frame whose return address
   is the NATIVE instruction of the resume point after the call, so a
   return lands back in the function, which picks up where it left off.
   An escape runs one instruction in the emulator, which then comes to
   the NATIVE instruction after it, so every instruction behaves, traps
   and all, exactly as in bytecode.  Everything that has to survive a
   call -- arguments, temporaries -- lives on the emulator's value
   stack, never in C variables, so the GC, call/cc and stack flushing
   all see a native frame exactly as they see a bytecode one.

   A function returns to the emulator after a call, return, trap or
   escape, saying what to do next: continue at e_pc, or call the
   operation on top of the value stack with e_nargs arguments.  This
   file is included by the generated C, so keep it to what that needs. */

#ifndef _OAK_NATIVE_H_INCLUDED
#define _OAK_NATIVE_H_INCLUDED

#include <signal.h>
#include <stdbool.h>
#include "config.h"
#include "data.h"
#include "stacks.h"
#include "weak.h"
#include "timers.h"
#include "cmdline.h"
#include "native-ops.h"

#ifdef THREADS
#error "native code is not supported with threads"
#endif

/* Bump when the layout of anything below, or of the stub code
   vectors, changes. */
#define OAK_NATIVE_ABI 5

typedef int (*oak_native_fn) (unsigned resume);

typedef struct {
  const char *name;
  oak_native_fn fn;
} oak_native_entry;

/* A call site's cache (CompilerPlan.md milestone 5): the operation a
   known global held last time, its method, and the native function to
   jump to.  Two kinds of site, told apart by y_type:

   - A lambda site (y_type 0): the operation carries its method, whose
     slot is never written after MAKE-LAMBDA, so a matching operation
     means the method, its code and its environment still match.

   - A generic site (y_type is the first argument's type): the method
     depends on that type, and ADD-METHOD can replace it, so a hit
     additionally requires the operation's own one-entry cache to
     witness the pair -- %INSTALL-METHOD-WITH-ENV parks that cache
     when it installs, so the site cannot outlive the method it
     caches.  The witness is shared per operation: a site whose
     operation is aimed at another type right now just takes the slow
     path, exactly as the one-entry cache alone would.

   Sites hold raw refs, which the GC does not trace, so every
   collection, world load and module registration flushes them all
   (oak_native_flush_sites); in between nothing moves.  A filled site
   with a null fn means "not directly callable, stop asking" -- until
   the global holds something else, or the witnessed method changes. */
typedef struct {
  ref_t epoch;			/* oak_native_epoch when filled, or 0 */
  ref_t op;
  ref_t y_type;			/* generic: first argument's type; 0: lambda */
  ref_t offset;			/* generic: the method's bp offset (a fixnum) */
  ref_t method;
  ref_t code;
  ref_t env;
  oak_native_fn fn;
} oak_native_site;

/* Each shared object defines one of these, named oak_native_module_info. */
typedef struct {
  unsigned abi;
  unsigned word_size;
  unsigned instrs_per_ref;
  unsigned count;
  const oak_native_entry *entries;
  oak_native_site *sites;
  unsigned site_count;
} oak_native_module;

/* A module's functions are ON_FN(0), ON_FN(1), ..., named after the
   ON_PREFIX the module defines. */
#define ON_PASTE2(a, b)	a ## _ ## b
#define ON_PASTE(a, b)	ON_PASTE2(a, b)
#define ON_FN(i)	ON_PASTE(ON_PREFIX, i)
#define ON_STR2(x)	#x
#define ON_STR(x)	ON_STR2(x)

/* A module built as a shared object exports the one name dlopen looks
   for; built with -DON_STATIC to link into an oakc executable, it is
   named after its prefix instead, so many can be linked together (the
   generated main.c registers them; see oak-embed.h).  In an
   executable the functions are extern too: whole-program compilation
   (oakc --whole-program) calls them across translation units. */
#ifdef ON_STATIC
#define OAK_NATIVE_MODULE_NAME ON_PASTE(ON_PREFIX, module)
#define ON_FN_DECL int
#else
#define OAK_NATIVE_MODULE_NAME oak_native_module_info
#define ON_FN_DECL static int
#endif

/* SITES is an array, never empty (the compiler allocates a spare). */
#define OAK_NATIVE_MODULE(entries, sites)				\
  const oak_native_module OAK_NATIVE_MODULE_NAME = {			\
    OAK_NATIVE_ABI, OAK_WORD_SIZE, INSTRS_PER_REF,			\
    sizeof (entries) / sizeof ((entries)[0]), (entries),		\
    (sites), sizeof (sites) / sizeof ((sites)[0]) }

/* What a native function asks the emulator to do next. */
#define OAK_NATIVE_CONTINUE	0	/* resume bytecode at e_pc */
#define OAK_NATIVE_FUNCALL	1	/* call the operation on top of the stack */

/* NATIVE k is opcode 35 + k / 256 with argument k % 256. */
#define ON_NATIVE_OPCODE 35
#define ON_NATIVE_OPCODES 8

extern volatile sig_atomic_t signal_poll_flag;

/* Something to deliver: a pending signal, or the alarm clock tripping.
   This is native code's POLL_SIGNALS: without it the preemption the
   thread system is built on (alarms, multiproc.oak) would never fire
   in compiled code. */
#define ON_PENDING()							\
  (signal_poll_flag							\
   || (oak_timer_increment						\
       && (oak_timer_counter += oak_timer_increment) > OAK_TIMER_LIMIT))
extern ref_t oak_native_epoch;
extern oak_native_fn *oak_native_fns;

/* Slow paths, in native.c.  They take and return the value stack
   pointer, which the generated code keeps in a local. */
extern void oak_native_site_fill(oak_native_site * s, ref_t v,
				 ref_t y_type);
extern ref_t *oak_native_unflush(ref_t * sp, int n);
extern ref_t *oak_native_flush(ref_t * sp);
extern ref_t *oak_native_push_slow(ref_t * sp, ref_t v);
extern ref_t *oak_native_pop_n(ref_t * sp, long n);
extern void oak_native_context_room(void);
extern void oak_native_bad_resume(unsigned k);
/* The target of a compiled-away direct call: its guard is the
   constant 0, so it is never reached, but musttail needs a real
   function to not call. */
extern int oak_native_dead(unsigned k);
extern ref_t *oak_native_cons(ref_t * sp);
extern ref_t *oak_native_make_cell(ref_t * sp);
extern ref_t *oak_native_make_lambda(ref_t * sp);
extern ref_t *oak_native_make_closed_environment(ref_t * sp, int n);

/* In loop.c: the method of OP for objects of type Y_TYPE, as a FUNCALL
   finds it (filling the operation's cache), or e_nil; *OFFSET gets its
   bp offset. */
extern ref_t oak_native_find_method(ref_t op, ref_t y_type, ref_t * offset);

/* Byte offset from e_code_segment (a tagged ref) of the instruction at
   index I of the stub: what a context frame holds for a return to it. */
#define ON_INSTR_OFFSET(i)						\
  ((long)(CODE_CODE_START_OFF * sizeof (ref_t)) - PTR_TAG		\
   + (long)sizeof (instr_t)						\
   * (((i) / INSTRS_PER_REF) * INSTR_STRIDE + (i) % INSTRS_PER_REF))

#define ON_INSTR_PC(i)							\
  ((instr_t *) ((char *)e_code_segment + ON_INSTR_OFFSET(i)))

/* The argument of the operand instruction at index I. */
#define ON_OPERAND(i)	(*ON_INSTR_PC(i) >> 8)

#define ON_VSTACK_END	(value_stack.bp + value_stack.size)

#define ON_ENTER	ref_t *sp = value_stack.sp
#define ON_SYNC		(value_stack.sp = sp)

/* Make sure the buffer holds the element N below the top. */
#define ON_NEED(n)							\
  do { if ((long)(n) > (long)(sp - value_stack.bp))			\
	 sp = oak_native_unflush(sp, (n)); } while (0)

#define ON_PUSH(v)							\
  do { ref_t _on_v = (v);						\
       if (sp + 1 < ON_VSTACK_END) *++sp = _on_v;			\
       else sp = oak_native_push_slow(sp, _on_v); } while (0)

/* Room for N direct pushes with no flush in between, made while no C
   local holds the only copy of a ref: the C-locals emitter
   (#*native-ir?, native-compiler.oak) reserves at the start of a run,
   because a flush can collect, and a collector must never run while a
   value lives only in a C variable.  A flush leaves the buffer at its
   fill target, far more than any run's eight locals. */
#define ON_RESERVE(n)							\
  do { if (sp + (n) >= ON_VSTACK_END) sp = oak_native_flush(sp); } while (0)

#define ON_POP(n)							\
  do { long _on_n = (n);						\
       if (_on_n < (long)(sp - value_stack.bp + 1)) sp -= _on_n;	\
       else sp = oak_native_pop_n(sp, _on_n); } while (0)

#define ON_CONST(slot)	(REF_TO_PTR(e_code_segment)[(slot)])

#define ON_BOOL(c)	((c) ? e_t : e_nil)

#define ON_FIXNUMS(a, b)	((((a) | (b)) & TAG_MASK) == INT_TAG)

/* Is the fixnum arithmetic result R (a C integer) in range? */
#define ON_FITS(r)							\
  ((((size_t)(r)) >> (OAK_WORD_SIZE - (TAGSIZE + 1))) == 0		\
   || (((size_t)(r)) >> (OAK_WORD_SIZE - (TAGSIZE + 1))) == 7)

/* The product of fixnums X and Y into R, true if it is one too. */
#if OAK_WORD_SIZE == 64 && defined(HAVE___INT128)
#define ON_TIMES(r, x, y)						\
  ((r) = (ssize_t)((__int128)REF_TO_INT(x) * REF_TO_INT(y)),		\
   (__int128)REF_TO_INT(x) * REF_TO_INT(y) == (__int128)(r) && ON_FITS(r))
#else
#define ON_TIMES(r, x, y)						\
  (!__builtin_mul_overflow(REF_TO_INT(x), REF_TO_INT(y), &(r)) && ON_FITS(r))
#endif

#define ON_PUSH_CONTEXT(k)						\
  do { ref_t *_on_c = context_stack.sp;					\
       if (_on_c + CONTEXT_FRAME_SIZE						\
	   >= context_stack.bp + context_stack.size)			\
	 { ON_SYNC; oak_native_context_room();				\
	   _on_c = context_stack.sp; sp = value_stack.sp; }		\
       _on_c[1] = INT_TO_REF(ON_INSTR_OFFSET(k));			\
       _on_c[2] = e_current_method;					\
       _on_c[3] = PTR_TO_LOC(e_bp);					\
       context_stack.sp = _on_c + CONTEXT_FRAME_SIZE; } while (0)

static inline void
oak_native_pop_context(void)
{
  ref_t *c, *m;

  if (CONTEXT_FRAME_SIZE > (long)(context_stack.sp - context_stack.bp))
    stack_unflush(&context_stack, CONTEXT_FRAME_SIZE);
  c = context_stack.sp;
  e_bp = LOC_TO_PTR(c[0]);
  e_current_method = c[-1];
  m = REF_TO_PTR(e_current_method);
  e_code_segment = m[METHOD_CODE_OFF];
  e_env = REF_TO_PTR(m[METHOD_ENV_OFF]);
  e_pc = (instr_t *) ((char *)e_code_segment + REF_TO_INT(c[-2]));
  context_stack.sp = c - CONTEXT_FRAME_SIZE;
}

/* Find the C function for the stub code vector SEG by name and record
   it there; NULL if it is not a stub or its module is missing. */
extern oak_native_fn oak_native_resolve(ref_t seg);

/* The function for the stub code vector SEG: the recorded one, or
   freshly resolved (a fresh process, a reloaded module). */
static inline oak_native_fn
oak_native_known(ref_t seg)
{
  ref_t *cv = REF_TO_PTR(seg);
  size_t n = REF_TO_INT(cv[1]);

  if ((*CODE_SEG_FIRST_INSTR(seg) & 0xFF) != (ON_NATIVE_OPCODE << 2))
    return NULL;
  if (cv[n - 1] != oak_native_epoch)
    return oak_native_resolve(seg);
  return oak_native_fns[REF_TO_INT(cv[n - 3])];
}

static inline ref_t
oak_native_type(ref_t x)
{
  if (x & 0x1)
    return (x & 0x2) ? REF_SLOT(x, 0)
      : *(e_subtype_table + ((x & SUBTAG_MASK) / 4));
  return (x & 0x2) ? e_loc_type : e_fixnum_type;
}

/* Calls.

   A call does what the emulator's FUNCALL does with the operation on
   top of the stack, in C: a lambda's method, or a generic operation's
   method for the type of the first argument, found through the
   operation's method cache.  If the method is bytecode, the function
   returns to the emulator at its first instruction.  If it is native,
   then where the C compiler can guarantee tail calls (the musttail
   attribute) the function jumps straight to it, and otherwise returns
   to the emulator at its NATIVE 0, which is a trampoline: either way
   the C stack never grows.  A return likewise jumps straight to the
   native function it lands in.  Anything unusual -- no method, not an
   operation, a pending signal -- goes to the emulator's own FUNCALL,
   which deals with it.  Defining OAK_NATIVE_TRAMPOLINE forces the
   trampoline. */

/* 32-bit PowerPC cannot tail-call indirectly, so the attribute's
   presence is not enough there; the trampoline serves instead. */
#if !defined(OAK_NATIVE_TRAMPOLINE) && defined(__has_attribute) \
  && !(defined(__powerpc__) && !defined(__powerpc64__))
#if __has_attribute(musttail)
#define ON_MUSTTAIL __attribute__((musttail))
#endif
#endif

/* Set up the registers to call the operation on top of the stack.
   Return 0 if the emulator (or the call driver) should deal with it,
   1 if the method is bytecode (e_pc is at its start), or 2 if it is
   native (*FN gets its function, e_pc is past its NATIVE 0).  The
   driver passes IGNORE_SIGNAL, having taken care of the signal
   itself. */
static inline int
oak_native_call_1(oak_native_fn * fn, int ignore_signal)
{
  ref_t *sp = value_stack.sp;
  ref_t op = sp[0], m, seg;

  if ((!ignore_signal && ON_PENDING()) || !TAG_IS(op, PTR_TAG))
    return 0;
  m = REF_SLOT(op, OPERATION_LAMBDA_OFF);
  if (m == e_false)
    {
      ref_t y, y_type, offset;

      if (sp - value_stack.bp < 1)
	return 0;
      y = sp[-1];
      y_type = e_nargs == 0 ? e_object_type : oak_native_type(y);
      if (y_type == REF_SLOT(op, OPERATION_CACHE_TYPE_OFF))
	{
	  m = REF_SLOT(op, OPERATION_CACHE_METH_OFF);
	  offset = REF_SLOT(op, OPERATION_CACHE_TYPE_OFF_OFF);
	}
      else if ((m = oak_native_find_method(op, y_type, &offset)) == e_nil)
	return 0;
      e_bp = REF_TO_PTR(y) + REF_TO_INT(offset);
    }
  else if (!TAG_IS(m, PTR_TAG) || REF_SLOT(m, 0) != e_method_type)
    return 0;
  seg = REF_SLOT(m, METHOD_CODE_OFF);
  e_current_method = m;
  e_env = REF_TO_PTR(REF_SLOT(m, METHOD_ENV_OFF));
  e_code_segment = seg;
  e_pc = CODE_SEG_FIRST_INSTR(seg);
  if ((*fn = oak_native_known(seg)) == NULL)
    return 1;
  e_pc += 1;
  return 2;
}

static inline int
oak_native_call(oak_native_fn * fn)
{
  return oak_native_call_1(fn, 0);
}

/* After a return: if it landed on a NATIVE instruction whose
   function is known, step over it and return the function, with its
   resume point in *K. */
static inline oak_native_fn
oak_native_direct_return(unsigned *k)
{
  instr_t i = *e_pc;
  unsigned op = ((i >> 2) & 0x3F) - ON_NATIVE_OPCODE;
  oak_native_fn fn;

  if (ON_PENDING() || op >= ON_NATIVE_OPCODES
      || (fn = oak_native_known(e_code_segment)) == NULL)
    return NULL;
  *k = (op << 8) | (i >> 8);
  e_pc += 1;
  return fn;
}

#ifdef ON_MUSTTAIL

#define ON_RETURN							\
  do { oak_native_fn _on_f; unsigned _on_k;				\
       ON_SYNC; oak_native_pop_context();				\
       if ((_on_f = oak_native_direct_return(&_on_k)) != NULL)		\
	 ON_MUSTTAIL return _on_f(_on_k);				\
       return OAK_NATIVE_CONTINUE; } while (0)

/* Call the operation on top of the stack, with e_nargs arguments. */
#define ON_FUNCALL							\
  do { oak_native_fn _on_f;						\
       ON_SYNC;								\
       switch (oak_native_call(&_on_f))					\
	 {								\
	 case 2: ON_MUSTTAIL return _on_f(0);				\
	 case 1: return OAK_NATIVE_CONTINUE;				\
	 default: return OAK_NATIVE_FUNCALL;				\
	 } } while (0)

#else

#define ON_RETURN							\
  do { ON_SYNC; oak_native_pop_context();				\
       return OAK_NATIVE_CONTINUE; } while (0)

/* Call the operation on top of the stack, with e_nargs arguments. */
#define ON_FUNCALL							\
  do { oak_native_fn _on_f;						\
       ON_SYNC;								\
       switch (oak_native_call(&_on_f))					\
	 {								\
	 case 2: e_pc -= 1; return OAK_NATIVE_CONTINUE;			\
	 case 1: return OAK_NATIVE_CONTINUE;				\
	 default: return OAK_NATIVE_FUNCALL;				\
	 } } while (0)

#endif

/* The registers and the jump of a statically resolved call
   (native-compiler.oak's direct calls): Y is the method to run, a
   guard having proved it is the one compiled against, and FN its C
   function, named at compile time.  Same shape as a site hit; in a
   trampoline build the stub's NATIVE 0 resolves the function
   instead. */
#ifdef ON_MUSTTAIL
#define ON_DIRECT_JUMP(y, fn)						\
  do { e_current_method = (y);						\
       e_code_segment = REF_SLOT((y), METHOD_CODE_OFF);			\
       e_env = REF_TO_PTR(REF_SLOT((y), METHOD_ENV_OFF));		\
       e_pc = CODE_SEG_FIRST_INSTR(e_code_segment) + 1;			\
       ON_SYNC;								\
       ON_MUSTTAIL return (fn)(0); } while (0)
#else
#define ON_DIRECT_JUMP(y, fn)						\
  do { e_current_method = (y);						\
       e_code_segment = REF_SLOT((y), METHOD_CODE_OFF);			\
       e_env = REF_TO_PTR(REF_SLOT((y), METHOD_ENV_OFF));		\
       e_pc = CODE_SEG_FIRST_INSTR(e_code_segment);			\
       ON_SYNC;								\
       return OAK_NATIVE_CONTINUE; } while (0)
#endif

/* The common tail of a site hit: the registers, then the jump. */
#ifdef ON_MUSTTAIL
#define ON_SITE_JUMP(s)							\
  do { e_current_method = (s)->method;					\
       e_code_segment = (s)->code;					\
       e_env = REF_TO_PTR((s)->env);					\
       e_pc = CODE_SEG_FIRST_INSTR((s)->code) + 1;			\
       ON_SYNC;								\
       ON_MUSTTAIL return (s)->fn(0); } while (0)
#else
#define ON_SITE_JUMP(s)							\
  do { e_current_method = (s)->method;					\
       e_code_segment = (s)->code;					\
       e_env = REF_TO_PTR((s)->env);					\
       e_pc = CODE_SEG_FIRST_INSTR((s)->code);				\
       ON_SYNC;								\
       return OAK_NATIVE_CONTINUE; } while (0)
#endif

/* A call through the site cache S: V, the operation on top of the
   stack, was just loaded from a global.  On a hit set up the registers
   as oak_native_call_1 would and jump straight to the cached function;
   on anything else fall through to the ON_FUNCALL that follows every
   use of this.  A site refills when the global holds another
   operation, or on the witness of the operation's own cache: aimed at
   this call's type but disagreeing with the site (a first fill, or a
   method change).  When the cache is aimed elsewhere the site just
   falls through -- so a site whose argument types alternate costs
   compares, never fills, exactly as the one-entry cache alone would
   have behaved.  V and the argument are read from the stack, not from
   C locals: pushing them may have collected garbage, which moves the
   operation and flushes every site. */
#define ON_SITE_CALL(s, v)						\
  do { ref_t _on_v = (v), _on_t = INT_TO_REF(0);			\
       if (sp - value_stack.bp >= 1)					\
	 _on_t = e_nargs == 0 ? e_object_type				\
	   : oak_native_type(sp[-1]);					\
       if ((s)->epoch != oak_native_epoch || (s)->op != _on_v)		\
	 oak_native_site_fill((s), _on_v, _on_t);			\
       else if ((s)->y_type == INT_TO_REF(0))				\
	 { if ((s)->fn != NULL && !ON_PENDING())			\
	     ON_SITE_JUMP(s); }						\
       else if (REF_SLOT(_on_v, OPERATION_CACHE_TYPE_OFF) == _on_t)	\
	 { if (_on_t != (s)->y_type					\
	       || REF_SLOT(_on_v, OPERATION_CACHE_METH_OFF) != (s)->method) \
	     oak_native_site_fill((s), _on_v, _on_t);			\
	   else if ((s)->fn != NULL && !ON_PENDING())			\
	     { e_bp = REF_TO_PTR(sp[-1]) + REF_TO_INT((s)->offset);	\
	       ON_SITE_JUMP(s); } }					\
     } while (0)

/* Do what the emulator does when an argless instruction INSTR fails
   on the NARGS arguments on top of the stack: call its trap operation
   on them, returning to resume point K with the result in their
   place. */
#ifndef FAST
#define ON_TRAP_NOTE(kind, i, n)					\
  do { if (trace_traps)							\
	 fprintf(stderr, "NTRAP %s %d %d\n", (kind), (int)(i),		\
		 (int)(n)); } while (0)
#else
#define ON_TRAP_NOTE(kind, i, n)
#endif

#define ON_TRAP(instr, nargs, k)					\
  do { ON_TRAP_NOTE("a", instr, nargs);					\
       ON_PUSH_CONTEXT(k);						\
       ON_PUSH(e_argless_tag_trap_table[(instr)]);			\
       e_nargs = (nargs);						\
       ON_FUNCALL; } while (0)

/* CHECK-NARGS (op 24) or CHECK-NARGS-GTE (op 25) with argument N
   failed: as in the emulator, call the trap operation with N and the
   operation, the arguments still under them, and no new context. */
#define ON_NARGS_TRAP(op, n)						\
  do { unsigned _on_na = e_nargs;					\
       ON_TRAP_NOTE("n", op, n);					\
       ON_PUSH(INT_TO_REF(n));						\
       ON_PUSH(e_arged_tag_trap_table[(op)]);				\
       e_nargs = _on_na + 2;						\
       ON_FUNCALL; } while (0)

/* An arged instruction OP with argument AF failed on the NARGS
   arguments on top of the stack: call its trap operation on them, the
   argument and the operation, returning to resume point K with the
   result in their place. */
#define ON_ATRAP(op, af, nargs, k)					\
  do { unsigned _on_na = (nargs);					\
       ON_TRAP_NOTE("r", op, _on_na);					\
       ON_PUSH_CONTEXT(k);						\
       ON_PUSH(INT_TO_REF(af));						\
       ON_PUSH(e_arged_tag_trap_table[(op)]);				\
       e_nargs = _on_na + 1;						\
       ON_FUNCALL; } while (0)

/* The same without a context frame: what the emulator does for the
   ops FUNCALL through STORE-SLOT (20..26 except 23), whose trap
   operation returns for them. */
#define ON_ATRAP_TAIL(op, af, nargs)					\
  do { unsigned _on_na = (nargs);					\
       ON_TRAP_NOTE("t", op, _on_na);					\
       ON_PUSH(INT_TO_REF(af));						\
       ON_PUSH(e_arged_tag_trap_table[(op)]);				\
       e_nargs = _on_na + 1;						\
       ON_FUNCALL; } while (0)

/* ^SUPER-TAIL: oak_rt_super_call found the method (shaped like
   oak_native_call), or asks for the trap with the type back on the
   stack. */
#ifdef ON_MUSTTAIL
#define ON_SUPER_TAIL(ktrap)						\
  do { oak_rt_fn _on_f;							\
       ON_SYNC;								\
       switch (oak_rt_super_call(&_on_f))				\
	 {								\
	 case 2: ON_MUSTTAIL return _on_f(0);				\
	 case 1: return OAK_NATIVE_CONTINUE;				\
	 default:							\
	   sp = value_stack.sp;						\
	   ON_TRAP(34, e_nargs + 2, ktrap);				\
	 } } while (0)
#else
#define ON_SUPER_TAIL(ktrap)						\
  do { oak_rt_fn _on_f;							\
       ON_SYNC;								\
       switch (oak_rt_super_call(&_on_f))				\
	 {								\
	 case 2: e_pc -= 1; return OAK_NATIVE_CONTINUE;			\
	 case 1: return OAK_NATIVE_CONTINUE;				\
	 default:							\
	   sp = value_stack.sp;						\
	   ON_TRAP(34, e_nargs + 2, ktrap);				\
	 } } while (0)
#endif

/* Control-transfer helpers (native-ops.h) leave the registers set;
   go back through the driver (or the emulator's NATIVE case). */
#define ON_DISPATCH							\
  do { return OAK_NATIVE_CONTINUE; } while (0)

/* Run the escape instruction at index I in the emulator, which then
   resumes the function at the NATIVE instruction after it. */
#define ON_ESCAPE(i)							\
  do { ON_SYNC; e_pc = ON_INSTR_PC(i);					\
       return OAK_NATIVE_CONTINUE; } while (0)

/* Before a backward branch to resume point K: if something is pending,
   go back to the dispatcher at NATIVE K, which delivers it and
   resumes. */
#define ON_POLL(k)							\
  do { if (ON_PENDING())						\
	 { ON_SYNC; e_pc = ON_INSTR_PC(k);				\
	   return OAK_NATIVE_CONTINUE; } } while (0)

#endif
