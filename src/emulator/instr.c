#define _REENTRANT

#include "data.h"

#ifndef FAST

#include <stdio.h>

#include "instr-data.c"

void
print_pc(u_int16_t *e_progc)
{
  if (SPATIC_PTR((ref_t *) e_progc))
    fprintf(stdout, "%7ld[spatic] ",
	    (long)((char *)e_progc - (char *)spatic.start));
  else
    fprintf(stdout, "%7ld[new   ] ",
	    (long)((char *)e_progc - (char *)new_space.start
		   + 4 * spatic.size));
}

void 
print_instr(int op_field, int arg_field, u_int16_t *e_progc)
{
  print_pc(e_progc);

  if (op_field == 0)
    fprintf(stdout, "%s\n", argless_instr_name[arg_field]);
  else
    fprintf(stdout, "%s %d\n", instr_name[op_field], arg_field);
}

#endif
