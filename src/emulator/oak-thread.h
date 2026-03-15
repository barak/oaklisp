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


/*
 * oak-thread.h - Portable thread primitives.
 *
 * Thin abstraction layer over pthreads (POSIX) or Windows threads.
 * Header-only: all definitions are typedefs, macros, or static
 * inline functions.
 *
 * POSIX path: used on Linux, macOS, and MinGW (which ships with
 * winpthreads).
 *
 * Windows path (MSVC): uses CRITICAL_SECTION, CONDITION_VARIABLE,
 * FlsAlloc, CreateThread, SwitchToThread.  Requires Windows Vista+.
 */

#ifndef OAK_THREAD_H_INCLUDED
#define OAK_THREAD_H_INCLUDED


#if defined(_MSC_VER) && !defined(__MINGW32__)
/* ------------------------------------------------------------------ */
/*  Windows (MSVC) — Vista+ required                                   */
/* ------------------------------------------------------------------ */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600   /* Vista */
#endif
#include <windows.h>
#include <process.h>


/* -- Mutex --------------------------------------------------------- */

typedef CRITICAL_SECTION oak_mutex_t;

/* No static initializer on Windows — use oak_mutex_init() instead.
   OAK_MUTEX_INITIALIZER is defined as a zeroed struct so that
   statically declared mutexes are at least zero-initialized; they
   MUST be dynamically initialized via oak_mutex_init() before use.
   Call oak_threads_system_init() from main() to handle this. */
#define OAK_MUTEX_INITIALIZER  {0}

static __inline void oak_mutex_init(oak_mutex_t *m) {
    InitializeCriticalSection(m);
}

static __inline void oak_mutex_destroy(oak_mutex_t *m) {
    DeleteCriticalSection(m);
}

static __inline void oak_mutex_lock(oak_mutex_t *m) {
    EnterCriticalSection(m);
}

static __inline void oak_mutex_unlock(oak_mutex_t *m) {
    LeaveCriticalSection(m);
}

/* Returns 0 on success (lock acquired), nonzero on failure. */
static __inline int oak_mutex_trylock(oak_mutex_t *m) {
    return TryEnterCriticalSection(m) ? 0 : 1;
}


/* -- Condition variable -------------------------------------------- */

typedef CONDITION_VARIABLE oak_cond_t;

#define OAK_COND_INITIALIZER  CONDITION_VARIABLE_INIT

static __inline void oak_cond_init(oak_cond_t *cv) {
    InitializeConditionVariable(cv);
}

static __inline void oak_cond_destroy(oak_cond_t *cv) {
    /* Windows CONDITION_VARIABLE has no destroy function. */
    (void)cv;
}

static __inline void oak_cond_wait(oak_cond_t *cv, oak_mutex_t *m) {
    SleepConditionVariableCS(cv, m, INFINITE);
}

static __inline void oak_cond_broadcast(oak_cond_t *cv) {
    WakeAllConditionVariable(cv);
}


/* -- Thread-local storage ------------------------------------------ */

/* Use Fiber Local Storage (FlsAlloc) instead of TlsAlloc because
   FLS supports per-key destructors, matching pthread_key_create. */

typedef DWORD oak_tls_key_t;

static __inline void oak_tls_create(oak_tls_key_t *key,
				    void (*destructor)(void *)) {
    *key = FlsAlloc((PFLS_CALLBACK_FUNCTION)destructor);
}

static __inline void *oak_tls_get(oak_tls_key_t key) {
    return FlsGetValue(key);
}

static __inline void oak_tls_set(oak_tls_key_t key, void *value) {
    FlsSetValue(key, value);
}


/* -- Thread creation ----------------------------------------------- */

typedef HANDLE oak_thread_t;

/* Wrapper so the signature matches _beginthreadex expectations.
   The user function has pthread-style void*(*)(void*) signature;
   _beginthreadex wants unsigned(__stdcall *)(void*). */
typedef struct {
    void *(*func)(void *);
    void *arg;
} oak_thread_trampoline_t;

static unsigned __stdcall oak_thread_trampoline_(void *p) {
    oak_thread_trampoline_t info = *(oak_thread_trampoline_t *)p;
    free(p);
    info.func(info.arg);
    return 0;
}

/* Returns 0 on success, nonzero on failure. */
static __inline int oak_thread_create(oak_thread_t *t,
				      void *(*func)(void *), void *arg) {
    oak_thread_trampoline_t *info =
	(oak_thread_trampoline_t *)malloc(sizeof(*info));
    if (!info) return 1;
    info->func = func;
    info->arg = arg;
    *t = (HANDLE)_beginthreadex(NULL, 0, oak_thread_trampoline_,
				info, 0, NULL);
    if (*t == 0) { free(info); return 1; }
    return 0;
}


/* -- Yield --------------------------------------------------------- */

static __inline void oak_thread_yield(void) {
    SwitchToThread();
}

/* Flag: Windows CRITICAL_SECTION cannot be statically initialized
   like PTHREAD_MUTEX_INITIALIZER.  Code that declares mutexes with
   OAK_MUTEX_INITIALIZER must call oak_mutex_init() on each before
   use.  oak_threads_system_init() handles this at startup. */
#define OAK_NEEDS_DYNAMIC_MUTEX_INIT 1


#else
/* ------------------------------------------------------------------ */
/*  POSIX (Linux, macOS, MinGW)                                        */
/* ------------------------------------------------------------------ */

#include <pthread.h>
#include <sched.h>


/* -- Mutex --------------------------------------------------------- */

typedef pthread_mutex_t oak_mutex_t;

#define OAK_MUTEX_INITIALIZER  PTHREAD_MUTEX_INITIALIZER

static inline void oak_mutex_init(oak_mutex_t *m) {
    pthread_mutex_init(m, NULL);
}

static inline void oak_mutex_destroy(oak_mutex_t *m) {
    pthread_mutex_destroy(m);
}

static inline void oak_mutex_lock(oak_mutex_t *m) {
    pthread_mutex_lock(m);
}

static inline void oak_mutex_unlock(oak_mutex_t *m) {
    pthread_mutex_unlock(m);
}

/* Returns 0 on success (lock acquired), nonzero on failure. */
static inline int oak_mutex_trylock(oak_mutex_t *m) {
    return pthread_mutex_trylock(m);
}


/* -- Condition variable -------------------------------------------- */

typedef pthread_cond_t oak_cond_t;

#define OAK_COND_INITIALIZER  PTHREAD_COND_INITIALIZER

static inline void oak_cond_init(oak_cond_t *cv) {
    pthread_cond_init(cv, NULL);
}

static inline void oak_cond_destroy(oak_cond_t *cv) {
    pthread_cond_destroy(cv);
}

static inline void oak_cond_wait(oak_cond_t *cv, oak_mutex_t *m) {
    pthread_cond_wait(cv, m);
}

static inline void oak_cond_broadcast(oak_cond_t *cv) {
    pthread_cond_broadcast(cv);
}


/* -- Thread-local storage ------------------------------------------ */

typedef pthread_key_t oak_tls_key_t;

static inline void oak_tls_create(oak_tls_key_t *key,
				  void (*destructor)(void *)) {
    pthread_key_create(key, destructor);
}

static inline void *oak_tls_get(oak_tls_key_t key) {
    return pthread_getspecific(key);
}

static inline void oak_tls_set(oak_tls_key_t key, void *value) {
    pthread_setspecific(key, value);
}


/* -- Thread creation ----------------------------------------------- */

typedef pthread_t oak_thread_t;

/* Returns 0 on success, nonzero on failure. */
static inline int oak_thread_create(oak_thread_t *t,
				    void *(*func)(void *), void *arg) {
    return pthread_create(t, NULL, func, arg);
}


/* -- Yield --------------------------------------------------------- */

static inline void oak_thread_yield(void) {
    sched_yield();
}


#endif /* _MSC_VER / POSIX */

#endif /* OAK_THREAD_H_INCLUDED */
