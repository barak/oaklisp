/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#define _REENTRANT

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <wait.h>
#include <pthread.h>
#include "config.h"
#include "data.h"
#include "cmdline.h"
#include "weak.h"
#include "stacks.h"
#include "worldio.h"
#include "loop.h"
#include "xmalloc.h"


int
main(int argc, char **argv)
{
#ifdef THREADS
  int my_index;
  int *my_index_p;
  pthread_key_create (&index_key, (void*)free_registers);
#endif
 
#ifdef THREADS
  my_index_p = (int *)malloc (sizeof (int));
  *my_index_p = get_next_index();
  pthread_setspecific (index_key, (void*)my_index_p);
  my_index_p = pthread_getspecific(index_key);
  my_index = *my_index_p;
  gc_ready[my_index] = 0;
  /* inc_next_index();*/
  value_stack_array[my_index] = (oakstack*)malloc (sizeof (oakstack));
  cntxt_stack_array[my_index] = (oakstack*)malloc(sizeof (oakstack));
  value_stack.size = 1024;
  value_stack.filltarget = 1024/2;
  context_stack.size = 512;
  context_stack.filltarget = 512/2;
  gc_examine_ptr = gc_examine_buffer;
#endif

  parse_cmd_line(argc, argv);

  init_weakpointer_tables();

  init_stacks();

  read_world(world_file_name);

  new_space.size = e_next_newspace_size = original_newspace_size;
  alloc_space(&new_space, new_space.size);
  free_point = new_space.start;

#ifdef THREADS
  register_array[my_index] = (register_set_t*)malloc(sizeof(register_set_t));
#else
  reg_set = (register_set_t*)malloc(sizeof(register_set_t));
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
