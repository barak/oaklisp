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


#ifndef _LINEEDIT_H_INCLUDED
#define _LINEEDIT_H_INCLUDED

#include <stdio.h>
#include "data.h"

/* getc(), except that on an interactive standard input, when the
   emulator was built with GNU readline, a whole line is read with
   editing and history and then handed out a character at a time. */
extern int oak_getc(FILE *f);

/* Remember what is being written to standard output, so the text of
   the current line (the prompt, usually) can be given to readline. */
extern void oak_note_putc(int c, FILE *f);

extern bool_int no_line_editing;

#endif
