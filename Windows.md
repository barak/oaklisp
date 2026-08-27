Building Oaklisp on Windows
============================

This document covers Windows-specific build issues for the Oaklisp
emulator and cold linker.  See also `Portability.md` for the full
history of the portability work.


Toolchain Options
==================

There are two ways to build Oaklisp on Windows:

1. **MinGW-w64** (GCC on Windows) -- the easier path.  MinGW-w64
   ships with winpthreads and GNU `<getopt.h>`, so it follows the
   same POSIX code paths as Linux and macOS.  The autotools build
   system works under MSYS2.

2. **MSVC** (Visual Studio / cl.exe) -- requires more care.  MSVC
   lacks POSIX headers and functions; the portability headers
   (`oak-atomic.h`, `oak-thread.h`, `oak-getopt.h`, `signals.h`)
   provide Windows-native alternatives.  There is no autotools
   support for MSVC; a manual `Makefile.msvc` or CMake file would
   be needed.


Minimum Requirements
=====================

- **Windows Vista or later** (Windows 7+ recommended).  The threading
  layer uses `CONDITION_VARIABLE` and `FlsAlloc`, which require
  Vista (`_WIN32_WINNT >= 0x0600`).

- **MSVC 2015 (v14.0) or later.**  Required for:
  - C99 designated initializers (`{ .field = val }`)
  - `%zu` / `%zd` format specifiers
  - `snprintf()` (older MSVC only had `_snprintf`)
  - `static inline` in C mode
  - `<stdatomic.h>` requires MSVC 17.5+ (VS 2022 17.5); older MSVC
    falls back to `_Interlocked*` intrinsics via `oak-atomic.h`


Building with MinGW-w64 (MSYS2)
=================================

Install MSYS2 (https://www.msys2.org/), then from the MSYS2 shell:

    pacman -S mingw-w64-x86_64-gcc make autoconf automake
    autoreconf --install
    ./configure
    make

MinGW uses the POSIX code paths throughout.  No Windows-specific
code is activated.  Thread support:

    ./configure --enable-threads
    make

The pthreads library is provided by MinGW's winpthreads.


Building with MSVC
===================

Autotools does not generate MSVC project files.  To build with MSVC
you need to compile the source files manually or create a build
script.  A minimal approach:

    cl /O2 /DHAVE_CONFIG_H /I. /I../.. ^
       /DDEFAULT_WORLD=\"oakworld.bin\" /DFAST ^
       cmdline.c data.c gc.c gc-ms.c instr.c loop.c ^
       oaklisp.c signals.c stacks.c threads.c timers.c ^
       weak.c worldio.c xmalloc.c ^
       /Fe:oaklisp.exe

For threaded builds, add `/DTHREADS /DUSE_MARK_SWEEP`.

For oak-cold-linker:

    cl /O2 oak-cold-linker.c /Fe:oak-cold-linker.exe

You will need a `config.h` that defines the appropriate feature
macros.  See "Generating config.h for MSVC" below.


Generating config.h for MSVC
==============================

Since autotools cannot run under native MSVC, you must create
`config.h` manually or use a template.  Key defines needed:

    #define PACKAGE_STRING "Oaklisp 2.0.0~devel"

    /* Pointer size: 8 for 64-bit, 4 for 32-bit */
    #define SIZEOF_VOID_P 8

    /* Word size: must match pointer size */
    #define __WORDSIZE 64

    /* Available headers */
    #define HAVE_STDDEF_H 1
    #define HAVE_STDLIB_H 1
    #define HAVE_STRING_H 1
    /* #undef HAVE_UNISTD_H */
    /* #undef HAVE_SYS_TIME_H */
    #define HAVE_WINDOWS_H 1

    /* stdatomic.h: define if using MSVC 17.5+ (VS 2022 17.5) */
    /* #define HAVE_STDATOMIC_H 1 */

    /* Available functions */
    #define HAVE_STRERROR 1
    /* #undef HAVE_GETRUSAGE */
    /* #undef HAVE_GETTIMEOFDAY */
    #define HAVE_GETTICKCOUNT 1
    /* #undef HAVE_CLOCK */

    /* Types */
    #define HAVE_LONG_LONG 1
    /* #undef HAVE___INT128 */

    /* Memory functions */
    #define HAVE_MALLOC 1
    #define HAVE_REALLOC 1

    /* Endianness: x86/x64 is little-endian */
    /* #undef WORDS_BIGENDIAN */


Portability Headers
====================

The following headers handle platform differences automatically.
They detect MSVC via `_MSC_VER` and MinGW via `__MINGW32__`.  MinGW
always uses the POSIX path.

oak-atomic.h
-------------

Provides portable atomic operations.  Three tiers:
1. C11 `<stdatomic.h>` (MSVC 17.5+ or when `HAVE_STDATOMIC_H` is
   defined)
2. GCC/Clang `__atomic_*` builtins
3. MSVC `_Interlocked*` intrinsics (automatic fallback)

Also provides `CLZ64`/`CTZ64` (uses `_BitScanReverse64`/
`_BitScanForward64` on MSVC) and `OAK_LIKELY`/`OAK_UNLIKELY`
(no-ops on MSVC).

oak-thread.h
--------------

Provides portable threading primitives.  On MSVC:

| Abstraction         | MSVC Implementation              |
|---------------------|----------------------------------|
| `oak_mutex_t`       | `CRITICAL_SECTION`               |
| `oak_cond_t`        | `CONDITION_VARIABLE`             |
| `oak_tls_key_t`     | `DWORD` (via `FlsAlloc`)         |
| `oak_thread_t`      | `HANDLE` (via `_beginthreadex`)  |
| `oak_thread_yield()` | `SwitchToThread()`              |

Important: `CRITICAL_SECTION` cannot be statically initialized.
Mutexes declared with `OAK_MUTEX_INITIALIZER` must be dynamically
initialized before use.  This is handled by:
- `oak_threads_system_init()` -- called from `main()`, initializes
  `gc_lock`, `alloc_lock`, `index_lock`, `test_and_set_locative_lock`
- `gc_safepoint_init()` -- initializes `gc_concurrent_lock` and
  `gc_concurrent_done_cv`

The `OAK_NEEDS_DYNAMIC_MUTEX_INIT` macro is defined automatically
on MSVC to enable this code.

Thread-local storage uses `FlsAlloc` (Fiber Local Storage) rather
than `TlsAlloc` because `FlsAlloc` supports per-key destructors,
matching the `pthread_key_create` API.

oak-getopt.h
--------------

Provides `getopt_long_only()`, `struct option`, `optarg`, `optind`,
`no_argument`, and `required_argument`.  On POSIX/MinGW, includes
the system `<getopt.h>`.  On MSVC, provides a self-contained
implementation.

signals.h
-----------

Provides `oak_jmp_buf`, `oak_setjmp()`, `oak_longjmp()`.  On MSVC
these expand to standard `jmp_buf`/`setjmp`/`longjmp`.  On POSIX
they expand to `sigjmp_buf`/`sigsetjmp`/`siglongjmp`.


POSIX Function Mappings
========================

Several POSIX functions have MSVC equivalents with different names.
These are handled by compat macros:

| POSIX function | MSVC equivalent | Where handled          |
|----------------|-----------------|------------------------|
| `isatty()`     | `_isatty()`     | `data.h` ISATTY macro  |
| `fileno()`     | `_fileno()`     | `data.h` ISATTY macro  |
| `strdup()`     | `_strdup()`     | `oak-cold-linker.c`    |

The `write()` call in the signal crash handler is replaced with
`fwrite()` to `stderr` on Windows (in `signals.c`).


Signal Handling
================

Windows signal support is more limited than POSIX:

- **No `sigaction`.**  The MSVC path uses plain `signal()`.  On MSVC,
  `signal()` auto-deregisters the handler after it fires (similar to
  POSIX `SA_RESETHAND`), which is the desired behavior.

- **No `SIGBUS`.**  Windows does not have `SIGBUS`.  Only `SIGSEGV`
  and `SIGFPE` are registered.  Memory access violations on Windows
  are reported as `SIGSEGV` (or via structured exception handling,
  which is not used here).

- **No `siglongjmp`.**  The crash handler uses `oak_longjmp()` which
  expands to plain `longjmp()` on MSVC.  This is safe because
  `signal()` on Windows does not mask signals the way POSIX does.


Timer Functions
================

The timer code in `timers.c` is already portable via configure
detection:

| Function          | Guard macro          | Windows fallback  |
|-------------------|----------------------|-------------------|
| `getrusage()`     | `HAVE_GETRUSAGE`     | not used          |
| `gettimeofday()`  | `HAVE_GETTIMEOFDAY`  | not used          |
| `GetTickCount()`  | `HAVE_GETTICKCOUNT`  | primary on Windows|
| `clock()`         | `HAVE_CLOCK`         | fallback          |

On Windows, `GetTickCount()` is the preferred timer source and is
always available.


Known Limitations
==================

1. **No autotools on MSVC.**  The autotools build system (autoconf,
   automake) does not support MSVC natively.  A manual `config.h`
   and build script or a CMake migration would be needed for a
   proper MSVC build.

2. **World image portability.**  Binary world images (`.bin`) are
   not portable across word sizes or endianness.  A world built on
   64-bit Linux cannot be loaded on 32-bit Windows.  However, the
   `.oa` compiled bytecode files and `oak-cold-linker` are portable
   and can generate a fresh world on any platform.

3. **No structured exception handling (SEH).**  The crash handler
   uses C `signal()` rather than Windows SEH (`__try`/`__except`).
   This is sufficient for the current use case (catching SIGSEGV
   and SIGFPE to restart the VM loop) but may miss some Windows-
   specific exception types.

4. **Thread support is untested on Windows.**  The threading code
   (`--enable-threads`) has not been tested on Windows.  The
   portability abstractions are in place but may have edge cases.

5. **`%zu`/`%zd` format specifiers.**  Used in ~27 locations across
   the codebase for `size_t` formatting.  Requires MSVC 2015+.
   Older MSVC versions would need these replaced with casts to
   `unsigned long` and `%lu`, but this is not planned since
   MSVC 2015 is our minimum.
