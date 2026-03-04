Native Thread Support in Oaklisp
=================================

Status: EXPERIMENTAL / INCOMPLETE (disabled by default)

Enable with: `./configure --enable-threads`


Architecture
============

Two-level concurrency model:

1. **Heavyweight threads** — OS pthreads, each with its own VM
   registers, value stack, context stack, and GC buffer.  Created via
   `%make-heavyweight-thread` (opcode 70).

2. **Lightweight processes** — Oaklisp-level continuations multiplexed
   within a single pthread via a cooperative scheduler with
   timer-driven preemption.  Context switch via `pause()` using
   `call/cc`.

Each pthread runs its own lightweight process scheduler.  The global
scheduler queue (`*scheduleQ*`) is shared across all pthreads,
protected by a mutex.


C Emulator (`src/emulator/`)
============================

Key files and `#ifdef THREADS` sections:

| File | Thread-related content |
|------|----------------------|
| `threads.h` | Mutex declarations, `THREADY()` macro, TLS key |
| `threads.c` | `create_thread()`, `init_thread()`, `wait_for_gc()`, `set_gc_flag()` |
| `data.h` | Per-thread register macros, `ALLOCATE_PROT` spin-lock, GC buffer arrays |
| `loop.c` | Opcodes 67-72, `POLL_GC_SIGNALS()`, thread index init |
| `gc.c` | `FORTHREADS` macro, per-thread root scanning, GC coordination |
| `oaklisp.c` | Thread-0 TLS init, per-thread stack/register allocation |
| `stacks.c` | Thread index retrieval in `init_stacks()` |
| `config.h` | `MAX_THREAD_COUNT` (200), disables move-to-front under threads |

Per-thread state (via TLS `index_key`):

    register_array[my_index]        VM registers (e_pc, e_env, e_bp, ...)
    value_stack_array[my_index]     Value stack
    cntxt_stack_array[my_index]     Context stack
    gc_examine_buffer_array[my_index]  GC worklist

Global shared state:

    Heap (new_space, old_space)     Protected by alloc_lock
    free_point, new_space.end       Modified under alloc_lock
    gc_pending, gc_ready[]          GC coordination flags
    All Oaklisp objects             Mostly UNPROTECTED

GC is stop-the-world: one thread sets `gc_pending`, all others call
`wait_for_gc()` (blocking on `gc_lock`), GC thread scans all threads'
roots, then releases.

Thread-related opcodes:

| Opcode | Name | Effect |
|--------|------|--------|
| 67 | ENABLE-ALARMS | `timer_increment = 1` (enable preemption) |
| 68 | DISABLE-ALARMS | `timer_increment = 0` (disable preemption) |
| 69 | RESET-ALARM-COUNTER | `timer_counter = 0` |
| 70 | HEAVYWEIGHT-THREAD | Create pthread running given thunk; returns #t/#f |
| 71 | TEST-AND-SET-LOCATIVE | Atomic CAS on locative cell (mutex primitive) |
| 72 | THREAD-YIELD | `sched_yield()` under THREADS, no-op otherwise |


Oaklisp Runtime (`src/world/`)
==============================

| File | Content |
|------|---------|
| `multi-em.oak` | Primitive ops: `%make-heavyweight-thread`, `%load-process`, `%store-process`, `%test-and-set-locative`, `%thread-yield` |
| `multiproc.oak` | Queues, process descriptors, mutexes, scheduler, semaphores, futures (464 lines) |
| `multi-off.oak` | Guard variable `%no-threading` for cold-boot |
| `multiproc-tests.oak` | Basic tests + TODO list of known issues |

Synchronization primitives (all in `multiproc.oak`):

- **Mutex** (lines 130-143): Spin-lock via `%test-and-set-locative`.
  Yields between attempts via `%thread-yield` (opcode 72).
- **Semaphore** (lines 358-401): Mutex + waiting queue.  Blocked
  processes enqueued, woken on `signal`.
- **Futures** (lines 409-463): Lazy evaluation with dependent process
  queue; auto-scheduled computation.
- **Scheduler** (lines 240-264): Global `*scheduleQ*` protected by
  mutex + alarm disable.  `pause()` does `call/cc` context switch.


Known Bugs
==========

~~Race condition in TEST-AND-SET-LOCATIVE (loop.c:1428)~~
------------------------------------------------------
**FIXED.** Removed the racy unprotected pre-check before the lock.
The comparison now happens entirely inside the critical section.

~~Race condition in GC (gc.c:457, FORTHREADS macro)~~
--------------------------------------------------
**FIXED.** `next_index` is now snapshotted into `gc_thread_count` at
GC start.  All `FORTHREADS` loops iterate over the snapshot, preventing
reads of uninitialized `gc_ready[]` slots if a thread is created during
GC.

~~configure.ac pthread linkage bug (line 130)~~
--------------------------------------------
**FIXED.** Changed `$threads` to `$enable_threads` so `-lpthread` is
linked when `--enable-threads` is used.

~~Thread termination segfaults (multi-em.oak:27)~~
-----------------------------------------------
**FIXED.** `init_thread()` now prints a warning and exits cleanly if
the thunk returns, instead of segfaulting.  Comment in `multi-em.oak`
updated accordingly.

~~pthread_create error leak (threads.c:77)~~
-----------------------------------------
**FIXED.** `info_p` is now freed on `pthread_create()` failure.

~~Fluid bindings incomplete (multiproc.oak:175)~~
----------------------------------------------
**FIXED.** `setup-initial-process-object` re-enabled as a warm boot
action.  Sets `%no-threading` to `#f` so `fluid.oak` uses per-process
bindings via the process register instead of falling back to globals.

~~Unprotected symbol table (symbols.oak)~~
------------------------------------------
**FIXED.** Added `*symbol-table-mutex*` guard variable (initialized
`#f` in `symbols.oak`, set to a real mutex in `multiproc.oak`).
`intern` and `gensym` conditionally acquire/release the mutex.

~~Unprotected add-method (kernel1-install.oak)~~
------------------------------------------------
**FIXED.** Added `*add-method-mutex*` guard variable (initialized
`#f` in `kernel1-install.oak`, set to a real mutex in `multiproc.oak`).
The critical section in `%install-method-with-env` that mutates
`operation-method-alist` is now protected.

~~Method cache disabled under threads (config.h, loop.c)~~
----------------------------------------------------------
**FIXED.** `OP_TYPE_METH_CACHE` is now always enabled.  Cache writes
use a seqlock pattern: method and offset are written first, then the
type field is stored last with `__atomic_store_n` (release semantics)
as a commit flag.  Cache reads load the type with `__atomic_load_n`
(acquire semantics), read method/offset, then re-verify the type to
ensure a consistent snapshot.  `OP_METH_ALIST_MTF` remains disabled
under threads since it mutates the shared alist structure.

~~Spin-lock CPU waste (multiproc.oak)~~
---------------------------------------
**FIXED.** `acquire-mutex` now calls `%thread-yield` (opcode 72,
`sched_yield()`) between spin attempts instead of busy-waiting in a
tight loop.


Remaining Unprotected Shared Data Structures
=============================================

- **Hash tables** — concurrent insert/lookup can corrupt (general
  hash tables beyond the symbol table are not yet protected)

The TODO list in `multiproc-tests.oak:67` acknowledges: "race
conditions (symbol tables, hash tables, add-method)".  Symbol tables
and add-method are now protected; general hash tables remain.


Performance Issues
==================

- **Global allocator lock**: All heap allocation contends on single
  `alloc_lock` via `pthread_mutex_trylock()` spin-loop in
  `ALLOCATE_PROT` (data.h:366).
- **Stop-the-world GC**: All threads pause for every collection.


Effort Estimate to Finish
=========================

Easy (days): **ALL DONE**
- ~~Fix configure.ac variable name ($threads -> $enable_threads)~~
- ~~Fix TEST-AND-SET-LOCATIVE race (move check inside lock)~~
- ~~Fix GC next_index race (snapshot + block creation during GC)~~
- ~~Fix pthread_create error cleanup~~
- ~~Wrap thread thunks to catch returns/exceptions~~

Medium (weeks): **ALL DONE**
- ~~Replace spin-locks with yielding locks (opcode 72 + %thread-yield)~~
- ~~Protect symbol table with mutex (guard variable pattern)~~
- ~~Make add-method thread-safe (guard variable pattern)~~
- ~~Re-enable method cache with atomic seqlock pattern~~
- ~~Complete fluid/dynamic binding support (enable setup-initial-process-object)~~
- ~~Unify C-level and Oaklisp-level process management (enabled via warm boot action)~~

Hard (months):
- Thread-local allocation buffers (TLABs) or concurrent allocator
- Concurrent or incremental GC (replace stop-the-world)
- Systematic audit of all mutable globals in the 22K-line runtime
  (general hash tables, I/O buffers, global lists remain unprotected)
- Thread-aware cold-boot sequence

The architecture (per-thread VM state, stop-the-world GC rendezvous,
two-level scheduling) is sound.  The hard part is the systematic audit
of every shared mutable data structure in the Oaklisp runtime.
