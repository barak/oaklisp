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


/* The C implementations, in native-ops.c, of the instructions whose
   translations are too big to write inline: what natively compiled
   code uses instead of escaping to the interpreter, so a stand-alone
   executable needs no interpreter at all.  Each helper mirrors its
   loop.c case exactly.

   Conventions: a helper takes the value stack pointer and gives it
   back, through *SPP when it can refuse.  A helper that can trap
   checks everything before touching anything and returns false, with
   the stack exactly as it was, so the caller can call the trap
   operation; the comment on each says the trap's argument count.
   Control-transfer helpers (throw, continue, ...) are called with the
   stack synchronized and leave the registers set for the caller to
   return OAK_NATIVE_CONTINUE. */

#ifndef _NATIVE_OPS_H_INCLUDED
#define _NATIVE_OPS_H_INCLUDED

#include <stdio.h>
#include "config.h"
#include "data.h"

/* Streams are passed to Oaklisp as fixnum-looking refs: 0, 1, 2 for
   the standard streams, the FILE pointer itself otherwise (aligned,
   so its low bits pass for a fixnum tag).  Shared with loop.c. */

static inline ref_t
file_to_ref(FILE * f)
{
  return f == stdin ? INT_TO_REF(0)
    : f == stdout ? INT_TO_REF(1)
    : f == stderr ? INT_TO_REF(2)
    : (ref_t) f;
}

static inline FILE *
ref_to_file(ref_t r)
{
  return r == INT_TO_REF(0) ? stdin
    : r == INT_TO_REF(1) ? stdout
    : r == INT_TO_REF(2) ? stderr
    : (FILE *) r;
}

#ifndef THREADS

typedef int (*oak_rt_fn) (unsigned resume);

extern ref_t *oak_rt_reverse_cons(ref_t * sp);
extern bool oak_rt_allocate(ref_t ** spp);	/* 2 */
extern bool oak_rt_vlen_allocate(ref_t ** spp);	/* 2 */
extern bool oak_rt_crunch(ref_t ** spp);	/* 2 */
extern void oak_rt_get_data(ref_t * sp);
extern bool oak_rt_assq(ref_t ** spp);	/* 2 */
extern bool oak_rt_ash(ref_t ** spp);	/* 2 */
extern ref_t *oak_rt_rot(ref_t * sp);
extern ref_t *oak_rt_getc(ref_t * sp);
extern bool oak_rt_putc(ref_t * sp);	/* 1 */
extern void oak_rt_load_length(ref_t * sp);

/* -1 when done; otherwise the trap's argument count. */
extern int oak_rt_stream_primitive(unsigned n, ref_t ** spp);

extern bool oak_rt_throw(void);	/* 2; synchronized, sets registers */
extern void oak_rt_fill_continuation(void);	/* synchronized */
extern bool oak_rt_continue(void);	/* 2; synchronized, sets registers */
extern void oak_rt_gc(int full);	/* synchronized; pushes #f */
extern void oak_rt_halt(int code);
extern bool oak_rt_dump_world(ref_t ** spp);	/* 2 */
extern bool oak_rt_load_world(ref_t ** spp);	/* 2; sets registers */

/* ^SUPER-TAIL's dispatch, mirroring oak_native_call's contract: 2 =
   native method, *FN set, registers set, e_pc past its NATIVE 0; 1 =
   bytecode method, registers set, e_pc at its start; 0 = trap, the
   type pushed back on the stack (e_nargs + 2 arguments). */
extern int oak_rt_super_call(oak_rt_fn * fn);

/* The alarm counters of natively compiled code.  Nothing polls them
   there yet (see ON_POLL), so alarms do not preempt native code; the
   instructions exist so the code runs. */
extern ref_t *oak_rt_enable_alarms(ref_t * sp);
extern ref_t *oak_rt_disable_alarms(ref_t * sp);
extern ref_t *oak_rt_reset_alarm_counter(ref_t * sp);

/* The call driver: runs a world whose code is all native, in place of
   the interpreter.  The caller has set the registers to the boot
   method, as loop() expects.  In embed-native.c-linked executables
   this is the only execution engine. */
extern void oak_native_run(void);

#endif

#endif
