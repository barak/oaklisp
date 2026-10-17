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


/* A stand-alone Oaklisp executable, as oakc builds one: the emulator
   (liboakrt.a), the natively compiled program, and a generated main.c
   with an oak_embedding holding the world image, the program's stub
   bytecode (its .oa text, which --load-embedded loads at startup) and
   the program's native modules, all compiled in.  See oakc.in. */

#ifndef _OAK_EMBED_H_INCLUDED
#define _OAK_EMBED_H_INCLUDED

#ifndef THREADS
#include "oak-native.h"
#endif

typedef struct {
  const unsigned char *world;	/* world image bytes */
  size_t world_size;
  const char *program;		/* .oa text of the program's files, in order */
  size_t program_size;
#ifndef THREADS
  const oak_native_module *const *modules;
#else
  const void *const *modules;
#endif
  unsigned module_count;
} oak_embedding;

/* The emulator's main, less main(): EMB is null for the plain emulator. */
extern int oak_main(int argc, char **argv, const oak_embedding *emb);

/* oak_main on the command line an executable gives its world:
   OAK_ARGS (Oaklisp-side switches, --load-embedded ... --exit),
   then "--", then the user's own arguments, which the program can
   read with the usual argument-line operations. */
extern int oak_main_embedded(int argc, char **argv, const oak_embedding *emb,
			     const char *const *oak_args, int n_oak_args);

/* The same, but through the native call driver: no interpreter is
   linked in, and everything the program runs must be native
   (embed-native.c; oakc builds use this). */
extern int oak_main_native(int argc, char **argv, const oak_embedding *emb,
			   const char *const *oak_args, int n_oak_args);

#endif
