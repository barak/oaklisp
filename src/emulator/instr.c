#include "data.h"

#ifndef FAST

#include <stdio.h>

#include "instr-data.c"

void
print_pc(u_int16_t *e_pc)
{
  if (SPATIC_PTR((ref_t *) e_pc))
    fprintf(stdout, "%7ld[spatic] ",
	    (long)((char *)e_pc - (char *)spatic.start));
  else
    fprintf(stdout, "%7ld[new   ] ",
	    (long)((char *)e_pc - (char *)new_space.start
		   + 4 * spatic.size));
}

void 
print_instr(int op_field, int arg_field, u_int16_t *e_pc)
{
  print_pc(e_pc);

  if (op_field == 0)
    fprintf(stdout, "%s\n", argless_instr_name[arg_field]);
  else
    fprintf(stdout, "%s %d\n", instr_name[op_field], arg_field);
}

#endif
