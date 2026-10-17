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
#include "loop.h"
#include "xmalloc.h"
#include "native.h"
#include "oak-embed.h"
#ifdef USE_MARK_SWEEP
#include "gc-ms.h"
#endif


#ifndef THREADS
/* Natively compiled modules resolve their oak_rt_* helpers against
   this executable, so the archive member holding them must be linked
   in even though the interpreter itself never calls them. */
void (*volatile oak_force_native_ops) (void) = oak_native_run;
#endif

int
oak_main(int argc, char **argv, const oak_embedding *emb)
{
#ifdef THREADS
  int my_index;
  int *my_index_p;
  oak_threads_system_init();
  oak_tls_create(&index_key, free_registers);
#endif

#ifdef THREADS
  my_index_p = (int *)xmalloc(sizeof (int));
  *my_index_p = get_next_index();
  oak_tls_set(index_key, (void*)my_index_p);
  my_index_p = oak_tls_get(index_key);
  my_index = *my_index_p;
  gc_ready[my_index] = 0;
  /* inc_next_index();*/
  value_stack_array[my_index] = (oakstack*)xmalloc(sizeof (oakstack));
  cntxt_stack_array[my_index] = (oakstack*)xmalloc(sizeof (oakstack));
  value_stack.size = 1024;
  value_stack.filltarget = 1024/2;
  context_stack.size = 512;
  context_stack.filltarget = 512/2;
  gc_examine_ptr = gc_examine_buffer;
#endif

  parse_cmd_line(argc, argv);

  init_weakpointer_tables();

  init_stacks();
  oak_native_init();

  if (emb != NULL)
    {
      unsigned i;

      for (i = 0; i < emb->module_count; i++)
	oak_native_register(emb->modules[i]);
      oak_embedded_program = emb->program;
      oak_embedded_program_size = emb->program_size;
    }
  oak_load_pending_native_modules();

  if (emb != NULL && emb->world != NULL)
    read_world_mem(emb->world, emb->world_size);
  else
    read_world(world_file_name);

  new_space.size = e_next_newspace_size = original_newspace_size;
  alloc_space(&new_space, new_space.size);
  free_point = new_space.start;

#ifdef USE_MARK_SWEEP
  ms_init();
#if defined(THREADS)
  gc_safepoint_init();
#endif
#endif

#ifdef THREADS
  register_array[my_index] = (register_set_t*)xmalloc(sizeof(register_set_t));
#else
  reg_set = (register_set_t*)xmalloc(sizeof(register_set_t));
#endif

  /* Set the registers to the boot code */

  e_current_method = e_boot_code;
  e_env = REF_TO_PTR(REF_SLOT(e_current_method, METHOD_ENV_OFF));
  e_code_segment = REF_SLOT(e_current_method, METHOD_CODE_OFF);
  e_pc = CODE_SEG_FIRST_INSTR(e_code_segment);

  /* Put a reasonable thing in e_bp to avoid confusing GC */
  e_bp = e_env;

  /* Tell the boot function the truth */
  e_nargs = 0;

  /* Big virtual machine interpreter loop */
  loop(INT_TO_REF(54321));

  return 0;
}

int
oak_main_embedded(int argc, char **argv, const oak_embedding *emb,
		  const char *const *oak_args, int n_oak_args)
{
  /* argv[0], the built-in Oaklisp switches, then "--" and the user's
     own arguments, kept out of switch processing (it stops at the
     --exit the built-in switches end with) but visible to the program
     through the argument-line operations. */
  char **nargv = xmalloc(((size_t)argc + n_oak_args + 2) * sizeof(char *));
  int i, j = 0;

  nargv[j++] = argv[0];
  for (i = 0; i < n_oak_args; i++)
    nargv[j++] = (char *)oak_args[i];
  nargv[j++] = "--";
  for (i = 1; i < argc; i++)
    nargv[j++] = argv[i];
  nargv[j] = NULL;
  return oak_main(j, nargv, emb);
}
