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


  /* Set the registers to the boot code */

  e_current_method = e_boot_code;
  e_env = REF_TO_PTR (REF_SLOT (e_current_method, METHOD_ENV_OFF));
  e_code_segment = REF_SLOT (e_current_method, METHOD_CODE_OFF);
  e_pc = CODE_SEG_FIRST_INSTR (e_code_segment);

  /* Put a reasonable thing in e_bp to avoid confusing GC */
  e_bp = e_env;

  /* Tell the boot function the truth */
  e_nargs = 0;

  /* Big virtual machine interpreter loop */
  loop();

  return 0;
}
