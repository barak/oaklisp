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


/* The entry point of a straight-C stand-alone executable (oakc): like
   oak_main, but execution goes through the native call driver
   (oak_native_run) instead of the interpreter, so linking against this
   file pulls in no interpreter at all -- loop.o stays out of the
   executable, which is easy to check: nm PROG | grep ' loop'. */

#define _REENTRANT

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "data.h"
#include "cmdline.h"
#include "weak.h"
#include "stacks.h"
#include "worldio.h"
#include "gc.h"
#include "xmalloc.h"
#include "native.h"
#include "oak-embed.h"

#ifdef THREADS

/* Stand-alone executables are not supported with threads; a threaded
   emulator build simply carries no native entry point. */

#else

int
oak_main_native(int argc, char **argv, const oak_embedding *emb,
		const char *const *oak_args, int n_oak_args)
{
  /* argv[0], the built-in switches, then "--" and the user's own
     arguments; see oak_main_embedded. */
  char **nargv = xmalloc(((size_t)argc + n_oak_args + 2) * sizeof(char *));
  int i, j = 0;
  unsigned m;

  nargv[j++] = argv[0];
  for (i = 0; i < n_oak_args; i++)
    nargv[j++] = (char *)oak_args[i];
  nargv[j++] = "--";
  for (i = 1; i < argc; i++)
    nargv[j++] = argv[i];
  nargv[j] = NULL;

  parse_cmd_line(j, nargv);

  init_weakpointer_tables();
  init_stacks();
  oak_native_init();

  for (m = 0; m < emb->module_count; m++)
    oak_native_register(emb->modules[m]);
  oak_embedded_program = emb->program;
  oak_embedded_program_size = emb->program_size;

  read_world_mem(emb->world, emb->world_size);

  new_space.size = e_next_newspace_size = original_newspace_size;
  alloc_space(&new_space, new_space.size);
  free_point = new_space.start;

  reg_set = (register_set_t *) xmalloc(sizeof(register_set_t));

  e_current_method = e_boot_code;
  e_env = REF_TO_PTR(REF_SLOT(e_current_method, METHOD_ENV_OFF));
  e_code_segment = REF_SLOT(e_current_method, METHOD_CODE_OFF);
  e_pc = CODE_SEG_FIRST_INSTR(e_code_segment);
  e_bp = e_env;
  e_nargs = 0;

  /* What loop() pushes on entry. */
  *++value_stack.sp = INT_TO_REF(4321);
  *++value_stack.sp = INT_TO_REF(54321);

  oak_native_run();
  return 0;
}

#endif /* THREADS */
