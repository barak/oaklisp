/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

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
#include <pthread.h>

void *
init_thread (void *start_function)
{
  init_weakpointer_tables ();

  init_stacks ();

  read_world (world_file_name);

  reg_set = (register_set_t*)malloc (sizeof (register_set_t));
  
  /*What is the current method ?*/
  reg_set->e_current_method = (ref_t)start_function;
  e_env = REF_TO_PTR (REF_SLOT (reg_set->e_current_method, METHOD_ENV_OFF));
  reg_set->e_code_segment = REF_SLOT (reg_set->e_current_method, METHOD_CODE_OFF);
  reg_set->e_pc = CODE_SEG_FIRST_INSTR (reg_set->e_code_segment);
  reg_set->e_bp = e_env;
  reg_set->e_nargs = 0;

  /* Big virtual machine interpreter loop */
  loop(reg_set);

  return 0;
}

int create_thread (ref_t start_function)
{
  pthread_t new_thread;
  /*  pthread_create (&new_thread, NULL, (void *)init_thread, (void *)&start_function);*/
  return 1;
}

int
get_byte_gender (void)
{
  /* Byte Gender Detection Routine */
  unsigned long a = 0x04030201ul;
  unsigned char *cp = (unsigned char *) &a;

  if (cp[0] == 0x01)
    return little_endian;
  else
    return big_endian;
}

int
main (int argc, char **argv)
{
 
  byte_gender = get_byte_gender ();

  parse_cmd_line (argc, argv);

  init_weakpointer_tables ();

  init_stacks ();

  read_world (world_file_name);

  new_space.size = e_next_newspace_size = original_newspace_size;
  alloc_space (&new_space, new_space.size);
  free_point = new_space.start;

#ifdef THREADS
  reg_set = (register_set_t*)malloc (sizeof (register_set_t));
  
  reg_set->e_current_method = e_boot_code;
  e_env = REF_TO_PTR (REF_SLOT (reg_set->e_current_method, METHOD_ENV_OFF));
  reg_set->e_code_segment = REF_SLOT (reg_set->e_current_method, METHOD_CODE_OFF);
  reg_set->e_pc = CODE_SEG_FIRST_INSTR (reg_set->e_code_segment);
  reg_set->e_bp = e_env;
  reg_set->e_nargs = 0;
#else

  reg_set = NULL;
  /* Set the registers to the boot code */

  e_current_method = e_boot_code;
  e_env = REF_TO_PTR (REF_SLOT (e_current_method, METHOD_ENV_OFF));
  e_code_segment = REF_SLOT (e_current_method, METHOD_CODE_OFF);
  e_pc = CODE_SEG_FIRST_INSTR (e_code_segment);

  /* Put a reasonable thing in e_bp to avoid confusing GC */
  e_bp = e_env;

  /* Tell the boot function the truth */
  e_nargs = 0;
#endif

  /* Big virtual machine interpreter loop */
  loop(reg_set);

  return 0;
}










