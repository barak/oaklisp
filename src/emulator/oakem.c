/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <wait.h>
#include "config.h"
#include "data.h"
#include "cmdline.h"
#include "weak.h"
#include "stacks.h"
#include "worldio.h"
#include "loop.h"
#include "xmalloc.h"
#include "threads.h"
#include <pthread.h>

int
get_byte_gender(void)
{
  /* Byte Gender Detection Routine */
  unsigned long a = 0x04030201ul;
  unsigned char *cp = (unsigned char *)&a;

  if (cp[0] == 0x01)
    return little_endian;
  else
    return big_endian;
}

void free_registers ()
{
}
int
main(int argc, char **argv)
{
#ifdef THREADS
  int my_index;
  int *my_index_p;
  pthread_key_create (&index_key, (void*)free_registers);
#endif
 
  byte_gender = get_byte_gender ();

 
#ifdef THREADS
  my_index_p = (int *)xmalloc (sizeof (int));
  *my_index_p = next_index;
  pthread_setspecific (index_key, (void*)my_index_p);
  my_index_p = pthread_getspecific(index_key);
  my_index = *my_index_p;
  next_index++;
  value_stack_array[my_index] = (stack_t*)xmalloc (sizeof (stack_t));
  cntxt_stack_array[my_index] = (stack_t*)xmalloc(sizeof (stack_t));
  value_stack.size = 1024;
  value_stack.filltarget = 1024/2;
  context_stack.size = 512;
  context_stack.filltarget = 512/2;
#endif

 parse_cmd_line (argc, argv);

  init_weakpointer_tables ();

  init_stacks();

  read_world(world_file_name);

  new_space.size = e_next_newspace_size = original_newspace_size;
  alloc_space(&new_space, new_space.size);
  free_point = new_space.start;

#ifdef THREADS
  register_array[my_index] = (register_set_t*)xmalloc(sizeof(register_set_t));
#else
  reg_set = (register_set_t*)xmalloc (sizeof(register_set_t));
#endif

#ifdef THREADS
  e_current_method = e_boot_code;
  e_env = REF_TO_PTR (REF_SLOT (e_current_method, METHOD_ENV_OFF));
  e_code_segment = REF_SLOT (e_current_method, METHOD_CODE_OFF);
  e_pc = CODE_SEG_FIRST_INSTR (e_code_segment);
  e_bp = e_env;
  e_nargs = 0;
  /* create_thread (NULL);*/

#else
  /* Set the registers to the boot code */

  e_current_method = e_boot_code;
  e_env = REF_TO_PTR(REF_SLOT(e_current_method, METHOD_ENV_OFF));
  e_code_segment = REF_SLOT(e_current_method, METHOD_CODE_OFF);
  e_pc = CODE_SEG_FIRST_INSTR(e_code_segment);

  /* Put a reasonable thing in e_bp to avoid confusing GC */
  e_bp = e_env;

  /* Tell the boot function the truth */
  e_nargs = 0;
#endif

  /* Big virtual machine interpreter loop */
  loop();

  return 0;
}










