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


/* Handle signals by polling.  In order to do this
   signal_poll_flag is set to > 0 when a signal comes in, and is
   checked and reset by the bytecode emulator at frequent intervals
   when it is safe to field an interrupt.

   BUG: This can delay interrupt handling when waiting for input.
 */

#define _REENTRANT

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "config.h"
#include "signal.h"



int signal_poll_flag = 0;

static void
intr_proc(int sig)
{
  signal_poll_flag++;
}


void
enable_signal_polling(void)
{
  signal_poll_flag = 0;
  if (signal(SIGINT, intr_proc) == SIG_ERR)
    fprintf(stderr, "Cannot enable signal polling.\n");
}

#if 0				/* the following is not used and commented out */

void
disable_signal_polling(void)
{
  signal_poll_flag = 0;
  if (signal(SIGINT, SIG_DFL) == SIG_ERR)
    fprintf(stderr, "Cannot disable signal polling.\n");
}

void
clear_signal(void)
{
  signal_poll_flag = 0;
}
#endif /* commented out */
