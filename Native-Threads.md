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
| `loop.c` | Opcodes 67-71, `POLL_GC_SIGNALS()`, thread index init |
| `gc.c` | `FORTHREADS` macro, per-thread root scanning, GC coordination |
| `oaklisp.c` | Thread-0 TLS init, per-thread stack/register allocation |
| `stacks.c` | Thread index retrieval in `init_stacks()` |
| `config.h` | `MAX_THREAD_COUNT` (200), disables method cache under threads |

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


Oaklisp Runtime (`src/world/`)
==============================

| File | Content |
|------|---------|
| `multi-em.oak` | Primitive ops: `%make-heavyweight-thread`, `%load-process`, `%store-process`, `%test-and-set-locative` |
| `multiproc.oak` | Queues, process descriptors, mutexes, scheduler, semaphores, futures (464 lines) |
| `multi-off.oak` | Guard variable `%no-threading` for cold-boot |
| `multiproc-tests.oak` | Basic tests + TODO list of known issues |

Synchronization primitives (all in `multiproc.oak`):

- **Mutex** (lines 130-143): Spin-lock via `%test-and-set-locative`.
  Busy-waits with no sleep/yield.
- **Semaphore** (lines 358-401): Mutex + waiting queue.  Blocked
  processes enqueued, woken on `signal`.
- **Futures** (lines 409-463): Lazy evaluation with dependent process
  queue; auto-scheduled computation.
- **Scheduler** (lines 240-264): Global `*scheduleQ*` protected by
  mutex + alarm disable.  `pause()` does `call/cc` context switch.


Known Bugs
==========

Race condition in TEST-AND-SET-LOCATIVE (loop.c:1428)
------------------------------------------------------
Non-atomic pre-check before acquiring `test_and_set_locative_lock`.
Another thread can modify the value between the check and the lock.
Fix: move the check inside the critical section.

Race condition in GC (gc.c:457, FORTHREADS macro)
--------------------------------------------------
`FORTHREADS` iterates `my_index` from 0 to `next_index`, but
`next_index` can change if a thread is created during GC.  Comment in
code acknowledges this.  Fix: snapshot `next_index` at GC start and
block thread creation during GC.

configure.ac pthread linkage bug (line 130)
--------------------------------------------
Tests `$threads` (undefined) instead of `$enable_threads`, so
`-lpthread` may not be linked even with `--enable-threads`.

Thread termination segfaults (multi-em.oak:27)
-----------------------------------------------
If a heavyweight thread's thunk returns, the VM crashes.  Comment
warns thunk must loop forever.

pthread_create error leak (threads.c:77)
-----------------------------------------
Comment: "Error creating --- need to add some clean up code here !!!"
malloc'd info struct not freed on failure.

Fluid bindings incomplete (multiproc.oak:175)
----------------------------------------------
Comment: "XXX removed to make it boot!!!"
`setup-initial-process-object` disabled.


Unprotected Shared Data Structures
===================================

The most dangerous category.  These are mutable globals accessed by
all threads with no synchronization:

- **Symbol tables** — interning a symbol mutates global hash table
- **Hash tables** — concurrent insert/lookup can corrupt
- **`add-method`** — modifying method dispatch tables while another
  thread is doing method lookup
- **Method cache** (`OP_TYPE_METH_CACHE`) — disabled under threads in
  `config.h` to avoid this, at a performance cost
- **Move-to-front optimization** (`OP_METH_ALIST_MTF`) — also disabled

The TODO list in `multiproc-tests.oak:67` acknowledges: "race
conditions (symbol tables, hash tables, add-method)".


Performance Issues
==================

- **Spin-lock mutexes**: `acquire-mutex` busy-waits with no sleep or
  yield.  Wastes CPU under contention.
- **Global allocator lock**: All heap allocation contends on single
  `alloc_lock` via `pthread_mutex_trylock()` spin-loop in
  `ALLOCATE_PROT` (data.h:366).
- **Method cache disabled**: Significant performance penalty since
  every method dispatch does full lookup.
- **Stop-the-world GC**: All threads pause for every collection.


Effort Estimate to Finish
=========================

Easy (days):
- Fix configure.ac variable name ($threads -> $enable_threads)
- Fix TEST-AND-SET-LOCATIVE race (move check inside lock)
- Fix GC next_index race (snapshot + block creation during GC)
- Fix pthread_create error cleanup
- Wrap thread thunks to catch returns/exceptions

Medium (weeks):
- Replace spin-locks with pthread_mutex + condition variables
- Protect symbol tables and hash tables with reader-writer locks
- Make add-method thread-safe
- Re-enable method cache with per-thread or lock-protected caching
- Unify C-level and Oaklisp-level process management
- Complete fluid/dynamic binding support

Hard (months):
- Thread-local allocation buffers (TLABs) or concurrent allocator
- Concurrent or incremental GC (replace stop-the-world)
- Systematic audit of all mutable globals in the 22K-line runtime
- Thread-aware cold-boot sequence

The architecture (per-thread VM state, stop-the-world GC rendezvous,
two-level scheduling) is sound.  The hard part is the systematic audit
of every shared mutable data structure in the Oaklisp runtime.
