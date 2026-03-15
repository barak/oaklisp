Thread Support in Oaklisp
==========================

Status: STABLE (disabled by default, enable with `--enable-threads`)

Enable with: `./configure --enable-threads`

Portable across Linux, macOS, and Windows (see `Portability.md` and
`Windows.md`).


Architecture
============

Two-level concurrency model:

1. **Heavyweight threads** (native OS threads) — each with its own VM
   registers, value stack, context stack, and GC buffer.  Created via
   `%make-heavyweight-thread` (opcode 70).  Uses pthreads on
   POSIX/MinGW, native Win32 threads on MSVC (via `oak-thread.h`).

2. **Lightweight processes** (green threads) — Oaklisp-level
   continuations multiplexed within a single native thread via a
   cooperative scheduler with timer-driven preemption.  Context switch
   via `pause()` using `call/cc`.

Each native thread runs its own lightweight process scheduler.  The
global scheduler queue (`*scheduleQ*`) is shared across all threads,
protected by a mutex.


C Emulator (`src/emulator/`)
============================

Key files and `#ifdef THREADS` sections:

| File | Thread-related content |
|------|----------------------|
| `threads.h` | Mutex declarations, `THREADY()` macro, TLS key, `oak_threads_system_init()` |
| `threads.c` | `create_thread()`, `init_thread()`, `wait_for_gc()`, `set_gc_flag()`, dynamic mutex init |
| `data.h` | Per-thread register macros, `ALLOCATE_PROT` spin-lock, GC buffer arrays |
| `loop.c` | Opcodes 67-72, `POLL_GC_SIGNALS()`, thread index init |
| `gc.c` | `FORTHREADS` macro, per-thread root scanning, GC coordination |
| `oaklisp.c` | Thread-0 TLS init, per-thread stack/register allocation |
| `stacks.c` | Thread index retrieval in `init_stacks()` |
| `config.h` | `MAX_THREAD_COUNT` (200), disables move-to-front under threads |
| `gc-ms.c` | Mark-sweep GC, TLABs, TLAB retire/refill, deferred objstart buffer |
| `gc-ms.h` | MS-GC and TLAB declarations, `TLAB_SIZE` (4096 words) |

Per-thread state (via TLS `index_key`):

    register_array[my_index]           VM registers (e_pc, e_env, e_bp, ...)
    value_stack_array[my_index]        Value stack
    cntxt_stack_array[my_index]        Context stack
    gc_examine_buffer_array[my_index]  GC worklist
    tlab_cursor_array[my_index]        TLAB bump pointer (USE_MARK_SWEEP only)
    tlab_end_array[my_index]           TLAB end pointer  (USE_MARK_SWEEP only)
    satb_buffer_array[my_index][]      SATB write barrier log (USE_MARK_SWEEP only)
    satb_count_array[my_index]         SATB buffer entry count (USE_MARK_SWEEP only)

Global shared state:

    Heap (new_space, old_space)     Protected by alloc_lock
    free_point, new_space.end       Modified under alloc_lock
    gc_pending, gc_ready[]          GC coordination flags
    objstart_bitmap, mark_bitmap    GC bitmaps (USE_MARK_SWEEP only)
    free_list_heads[]               Segregated free lists (USE_MARK_SWEEP only)

GC coordination: one thread sets `gc_pending`, all others call
`wait_for_gc()` (blocking on `gc_lock`).  Under `USE_MARK_SWEEP`, the
mark phase runs mostly concurrently with only brief STW pauses for
initial mark and remark; sweep runs lazily during allocation.  Without
`USE_MARK_SWEEP`, GC is fully stop-the-world (copying collector).

Thread-related opcodes:

| Opcode | Name | Effect |
|--------|------|--------|
| 67 | ENABLE-ALARMS | `timer_increment = 1` (enable preemption) |
| 68 | DISABLE-ALARMS | `timer_increment = 0` (disable preemption) |
| 69 | RESET-ALARM-COUNTER | `timer_counter = 0` |
| 70 | HEAVYWEIGHT-THREAD | Create native thread running given thunk; returns #t/#f |
| 71 | TEST-AND-SET-LOCATIVE | Atomic CAS on locative cell (mutex primitive) |
| 72 | THREAD-YIELD | `oak_thread_yield()` under THREADS, no-op otherwise |


Oaklisp Runtime (`src/world/`)
==============================

| File | Content |
|------|---------|
| `multi-em.oak` | Primitive ops: `%make-heavyweight-thread`, `%load-process`, `%store-process`, `%test-and-set-locative`, `%thread-yield`, `%threads-enabled?` |
| `multiproc.oak` | Queues, process descriptors, mutexes, scheduler, semaphores, futures |
| `multi-off.oak` | Guard variable `%no-threading` for cold-boot |

Synchronization primitives (all in `multiproc.oak`):

- **Mutex**: Spin-lock via `%test-and-set-locative`.  Yields between
  attempts via `%thread-yield` (opcode 72).
- **Semaphore**: Mutex + waiting queue.  Blocked processes enqueued,
  woken on `signal`.
- **Futures**: Lazy evaluation with dependent process queue;
  auto-scheduled computation.
- **Scheduler**: Global `*scheduleQ*` protected by mutex + alarm
  disable.  `pause()` does `call/cc` context switch.


Warm Boot Initialization
=========================

When the emulator is compiled with `--enable-threads`, thread
infrastructure is automatically initialized at warm boot:

1. `%threads-enabled?` (LOAD-REG 23) probes the emulator at warm boot
2. If threads are supported, `maybe-init-threads` (warm boot action in
   `multiproc.oak`) calls `init-thread-infrastructure`, which:
   - Creates a process descriptor and stores it in the process register
   - Sets `%no-threading` to `#f` (enables per-process fluid bindings)
   - Initializes all guard-variable mutexes
3. Extra native threads are spawned only if `--pthreads N` is used

On a non-THREADS emulator, `%threads-enabled?` returns nil and no
thread infrastructure is initialized — zero overhead.


Thread Safety
==============

All known shared data structures are protected:

**C emulator (protected by mutex or volatile):**
- `signal_poll_flag` — `volatile sig_atomic_t` for signal safety
- `alloc_lock` — protects `free_point`, `new_space.end`, and
  STORE-REG cases 14/15 in `loop.c`
- `wp_lock` — protects `ref_to_wp()` and `rebuild_wp_hashtable()` in
  `weak.c`
- `dump_lock` — protects DUMP-WORLD (case 73) and LOAD-WORLD (case 74)
  in `loop.c`

**Oaklisp runtime (protected by guard-variable mutex pattern):**
- `*symbol-table-mutex*` — `intern` and `gensym` in `symbols.oak`
- `*add-method-mutex*` — `%install-method-with-env` in `kernel1-install.oak`
- `*expression-table-mutex*` — `expression-table` and `fancy-accessor-*`
  lists in `anonymous.oak`
- `*stream-primitive-mutex*` — `sp-alist` cache in `streams.oak`
- `*locale-mutex*` — `(setter variable-here?)`, `(setter macro-here?)`,
  `(setter frozen-here?)` in `locales.oak`
- `*fluid-table-mutex*` — `add-to-current-fluid-bindings` in `fluid.oak`

All guard variables are `#f` during cold/warm boot (no mutex overhead)
and initialized to `(make mutex)` in `init-thread-infrastructure`
(`multiproc.oak`) when threading is activated.

**User responsibility:** General user-created hash tables used
concurrently need application-level locking.

**Method cache:** `OP_TYPE_METH_CACHE` is always enabled.  Cache writes
use a seqlock pattern with `OAK_ATOMIC_STORE_REF` (release) /
`OAK_ATOMIC_LOAD_REF` (acquire).  `OP_METH_ALIST_MTF` remains disabled
under threads since it mutates the shared alist structure.


Performance
===========

- **TLABs** (under `USE_MARK_SWEEP + THREADS`): Most allocations use
  a per-thread bump pointer with no lock.  Only TLAB refill and large
  allocations acquire `alloc_lock`.
- **Concurrent GC** (under `USE_MARK_SWEEP + THREADS`): The mark phase
  runs mostly concurrently (only two brief STW pauses for initial mark
  and remark), and sweep runs lazily during allocation.  The copying GC
  fallback remains STW but is only triggered for heap expansion.

See `GC.md` for full garbage collection documentation.


Testing
=======

Tested with the full system world (`oakworld.bin`) across all four
build configurations:

| Config | Flags | 20-test suite | GC stress | Compile | Dump/reload |
|--------|-------|---------------|-----------|---------|-------------|
| 1 | Default (copying GC) | 20/20 PASS | PASS | PASS | PASS |
| 2 | `USE_MARK_SWEEP` | 20/20 PASS | PASS | PASS | PASS |
| 3 | `THREADS` | 20/20 PASS | PASS | PASS | PASS |
| 4 | `THREADS + USE_MARK_SWEEP` | 20/20 PASS | PASS | PASS | PASS |

Additionally tested with `-O0` debug build (config 4) and 5 repeated
runs — all 20/20 PASS with no non-deterministic failures.


Future Enhancements
====================

Potential improvements, not required for current thread support:

- **Generational hints**: Track old->young pointers via a card table;
  skip scanning old-generation objects in minor collections
- **Parallel marking**: Multiple GC worker threads share the mark stack,
  reducing mark phase latency on multi-core systems
- **Incremental compaction**: Gradually relocate objects to reduce
  fragmentation without a full STW copying collection
