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


/* The emulator's side of natively compiled code (native.c). */

#ifndef _NATIVE_H_INCLUDED
#define _NATIVE_H_INCLUDED

#include "data.h"

extern void oak_native_init(void);
extern bool oak_native_load(const char *path);
extern bool oak_native_cc(const char *c_file, const char *so_file);
extern void oak_native_module_pending(const char *path);
extern void oak_load_pending_native_modules(void);

/* The stub bytecode of an embedded program (oak-embed.h), read by
   STREAM-PRIMITIVE 17; null in the plain emulator. */
extern const char *oak_embedded_program;
extern size_t oak_embedded_program_size;

extern void oak_native_register(const void *module_info);

/* Forget every direct-call site cache (oak-native.h): they hold raw
   refs and function pointers, so the GC, a world load and module
   registration each call this.  A no-op with THREADS. */
extern void oak_native_flush_sites(void);

#ifndef THREADS

#include "oak-native.h"

/* The C function for the stub code vector SEG, or NULL;
   oak_native_known (oak-native.h) resolves as needed nowadays. */
static inline oak_native_fn
oak_native_lookup(ref_t seg)
{
  return oak_native_known(seg);
}

#endif

#endif
