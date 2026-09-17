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


#ifndef _SIGNAL_H_INCLUDED
#define _SIGNAL_H_INCLUDED

#include <setjmp.h>
#include <signal.h>

void enable_signal_polling(void);
void disable_signal_polling(void);
void clear_signal(void);
extern volatile sig_atomic_t signal_poll_flag;

/* Crash recovery from fatal signals (SIGSEGV, SIGBUS, SIGFPE).
   On Windows, SIGBUS does not exist; only SIGSEGV and SIGFPE
   are handled.  Uses sigsetjmp/siglongjmp on POSIX, or
   setjmp/longjmp on Windows. */

#if defined(_MSC_VER) && !defined(__MINGW32__)
/* Windows (MSVC): no sigsetjmp/siglongjmp */
#define oak_jmp_buf       jmp_buf
#define oak_setjmp(buf)   setjmp(buf)
#define oak_longjmp(buf, val) longjmp(buf, val)
#else
/* POSIX: use sig-aware variants to save/restore signal mask */
#define oak_jmp_buf       sigjmp_buf
#define oak_setjmp(buf)   sigsetjmp(buf, 1)
#define oak_longjmp(buf, val) siglongjmp(buf, val)
#endif

/* Each thread runs its own copy of loop() and its own recovery point,
   so the jump buffer and the per-recovery counters have to be
   thread-local; a shared one would siglongjmp a faulting thread onto
   another thread's (possibly dead) stack frame. */
#ifdef THREADS
#if defined(_MSC_VER) && !defined(__MINGW32__)
#define OAK_THREAD_LOCAL __declspec(thread)
#else
#define OAK_THREAD_LOCAL __thread
#endif
#else
#define OAK_THREAD_LOCAL
#endif

extern OAK_THREAD_LOCAL oak_jmp_buf crash_jmpbuf;
extern OAK_THREAD_LOCAL volatile sig_atomic_t crash_signal;
extern OAK_THREAD_LOCAL volatile sig_atomic_t crash_count;
/* Per thread: each thread has its own recovery point, so whether
   recovery is armed is a property of the thread, not of the process. */
extern OAK_THREAD_LOCAL int crash_recovery_installed;
void enable_crash_recovery(void);
void reinstall_crash_handler(void);
void reset_crash_count(void);

#endif
