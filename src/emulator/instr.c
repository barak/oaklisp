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


#define _REENTRANT

#include "data.h"

#ifndef FAST

#include <stdio.h>

#include "instr-data.c"

void
print_pc(instr_t *e_progc)
{
  if (SPATIC_PTR((ref_t *) e_progc))
    fprintf(stdout, "%7ld[spatic] ",
	    (long)((char *)e_progc - (char *)spatic.start));
  else
    fprintf(stdout, "%7ld[new   ] ",
	    (long)((char *)e_progc - (char *)new_space.start
		   + sizeof(ref_t) * spatic.size));
}

/* ARG_FIELD runs to 255 and OP_FIELD to 63, but the tables generated
   into instr-data.c are only as long as there are instructions, so a
   malformed code vector would otherwise be traced by dereferencing
   whatever follows them. */

#define NELEM(a) ((int)(sizeof (a) / sizeof (a)[0]))

void
print_instr(int op_field, int arg_field, instr_t *e_progc)
{
  print_pc(e_progc);

  if (op_field == 0)
    {
      if (arg_field >= 0 && arg_field < NELEM(argless_instr_name))
	fprintf(stdout, "%s\n", argless_instr_name[arg_field]);
      else
	fprintf(stdout, "<unknown argless instruction %d>\n", arg_field);
    }
  else if (op_field > 0 && op_field < NELEM(instr_name))
    fprintf(stdout, "%s %d\n", instr_name[op_field], arg_field);
  else
    fprintf(stdout, "<unknown instruction %d> %d\n", op_field, arg_field);
}

#endif
