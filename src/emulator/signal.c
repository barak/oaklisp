/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/


/* Handle signals by polling.  In order to do this
   signal_poll_flag is set to > 0 when a signal comes in, and is
   checked and reset by the bytecode emulator at frequent intervals
   when it is safe to field an interrupt.

   BUG: This can delay interrupt handling when waiting for input.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "config.h"
#include "signal.h"


#ifndef _ICC


int signal_poll_flag = 0;

static void
intr_proc (int sig)
{
  sig = sig;			/* Keep the compiler from complaining */
  signal_poll_flag++;
}


void
enable_signal_polling (void)
{
  signal_poll_flag = 0;
  if (signal (SIGINT, intr_proc) == SIG_ERR)
    fprintf (stderr, "Cannot enable signal polling.\n");
}

#if 0				/* the following is not used and commented out */

void
disable_signal_polling (void)
{
  signal_poll_flag = 0;
  if (signal (SIGINT, SIG_DFL) == SIG_ERR)
    fprintf (stderr, "Cannot disable signal polling.\n");
}

void
clear_signal (void)
{
  signal_poll_flag = 0;
}
#endif /* commented out */

#endif
