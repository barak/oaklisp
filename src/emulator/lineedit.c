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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "config.h"
#include "data.h"
#include "xmalloc.h"
#include "lineedit.h"

#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

bool_int no_line_editing = false;

#ifdef HAVE_LIBREADLINE

/* The text written to standard output since the last newline. */
static char prompt[256];
static size_t prompt_len = 0;

/* The line most recently read, with its newline, and how much of it
   has been handed out. */
static char *line = NULL;
static size_t line_pos = 0;

void
oak_note_putc(int c, FILE *f)
{
  if (f != stdout)
    return;
  if (c == '\n' || c == '\r')
    prompt_len = 0;
  else if (prompt_len < sizeof(prompt) - 1)
    prompt[prompt_len++] = (char)c;
}

int
oak_getc(FILE *f)
{
  if (f != stdin || no_line_editing || !ISATTY(stdin))
    return getc(f);

  if (line == NULL)
    {
      char *l;

      /* Take over the terminal only from here: the prompt has already
	 been printed by Oaklisp, and the SIGINT handler is the
	 emulator's own, polled by the interpreter. */
      rl_catch_signals = 0;
      rl_already_prompted = 1;
      prompt[prompt_len] = '\0';
      fflush(stdout);
      l = readline(prompt);
      prompt_len = 0;
      if (l == NULL)
	return EOF;
      if (l[0] != '\0')
	add_history(l);
      line = xmalloc(strlen(l) + 2);
      strcpy(line, l);
      strcat(line, "\n");
      free(l);
      line_pos = 0;
    }

  {
    int c = (unsigned char)line[line_pos++];

    if (line[line_pos] == '\0')
      {
	free(line);
	line = NULL;
      }
    return c;
  }
}

#else

void
oak_note_putc(int c, FILE *f)
{
}

int
oak_getc(FILE *f)
{
  return getc(f);
}

#endif
