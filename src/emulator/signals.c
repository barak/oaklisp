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
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "config.h"
#include "signals.h"



volatile sig_atomic_t signal_poll_flag = 0;

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


/* Crash recovery from fatal signals (SIGSEGV, SIGBUS, SIGFPE).
   Uses oak_longjmp to jump back into the interpreter loop, which then
   routes through the tag-trap mechanism to an Oaklisp error handler.

   On POSIX: uses sigaction with SA_RESETHAND for auto-deregistration,
   and siglongjmp to restore the signal mask.

   On Windows: SIGBUS does not exist.  Uses signal() (the only option)
   and longjmp.  signal() on MSVC auto-deregisters the handler (like
   SA_RESETHAND), so reinstall_crash_handler() must be called after
   each recovery. */

oak_jmp_buf crash_jmpbuf;
volatile sig_atomic_t crash_signal = 0;
int crash_recovery_installed = 0;
volatile sig_atomic_t crash_count = 0;

#define MAX_CONSECUTIVE_CRASHES 3

static void
crash_handler(int sig)
{
  crash_signal = sig;
  if (crash_recovery_installed) {
    crash_count++;
    if (crash_count > MAX_CONSECUTIVE_CRASHES) {
      /* Repeated crashes — error handler itself is crashing.
         Bail out cleanly instead of producing garbage output. */
      const char msg[] = "\nFatal: repeated crashes during error recovery (possible deep stack overflow). Aborting.\n";
#ifdef _WIN32
      fwrite(msg, 1, sizeof(msg) - 1, stderr);
#else
      (void)!write(STDERR_FILENO, msg, sizeof(msg) - 1);
#endif
      _exit(1);
    }
    oak_longjmp(crash_jmpbuf, sig);
  }
  /* If recovery not installed, re-raise to get default behavior */
  signal(sig, SIG_DFL);
  raise(sig);
}


#if defined(_MSC_VER) && !defined(__MINGW32__)
/* ------------------------------------------------------------------ */
/*  Windows (MSVC): use signal()                                       */
/* ------------------------------------------------------------------ */

static void
install_one_crash_handler(int sig)
{
  signal(sig, crash_handler);
}

void
enable_crash_recovery(void)
{
  install_one_crash_handler(SIGSEGV);
  /* SIGBUS does not exist on Windows */
  install_one_crash_handler(SIGFPE);
  crash_recovery_installed = 1;
}

void
reinstall_crash_handler(void)
{
  install_one_crash_handler(SIGSEGV);
  install_one_crash_handler(SIGFPE);
}

#else
/* ------------------------------------------------------------------ */
/*  POSIX (Linux, macOS, MinGW): use sigaction()                       */
/* ------------------------------------------------------------------ */

static void
install_one_crash_handler(int sig)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = crash_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESETHAND;  /* auto-deregister after firing */
  sigaction(sig, &sa, NULL);
}

void
enable_crash_recovery(void)
{
  install_one_crash_handler(SIGSEGV);
  install_one_crash_handler(SIGBUS);
  install_one_crash_handler(SIGFPE);
  crash_recovery_installed = 1;
}

void
reinstall_crash_handler(void)
{
  install_one_crash_handler(SIGSEGV);
  install_one_crash_handler(SIGBUS);
  install_one_crash_handler(SIGFPE);
}

#endif /* _MSC_VER / POSIX */

void
reset_crash_count(void)
{
  crash_count = 0;
}
