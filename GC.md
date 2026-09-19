Oaklisp Garbage Collection
==========================

Oaklisp has two garbage collectors that coexist in the same codebase.
Which one is active depends on compile-time flags.  The two collectors
serve complementary roles: the mark-sweep GC handles normal collections
efficiently without moving objects, while the copying GC handles heap
compaction and expansion.


Build Configurations
====================

There are four build configurations, controlled by two C preprocessor
flags:

| Flag | Default | Effect |
|------|---------|--------|
| `USE_MARK_SWEEP` | off | Enable the mark-sweep GC for normal collections |
| `THREADS` | off | Enable pthread-based multi-threading |

The four resulting configurations:

| # | Flags | Primary GC | Allocation | Threading |
|---|-------|-----------|------------|-----------|
| 1 | (none) | Copying | Bump pointer | Single |
| 2 | `USE_MARK_SWEEP` | Mark-sweep (STW) | Free list + bump | Single |
| 3 | `THREADS` | Copying | Bump pointer + `alloc_lock` | Multi |
| 4 | `USE_MARK_SWEEP + THREADS` | Mark-sweep (concurrent) | TLABs + free list + bump | Multi |

How to build each:

```sh
# After ./configure:

# Config 1: Default (copying GC, single-threaded)
make

# Config 2: Mark-sweep, single-threaded
make CPPFLAGS="-DUSE_MARK_SWEEP"

# Config 3: Copying GC, multi-threaded
./configure --enable-threads
make

# Config 4: Concurrent mark-sweep, multi-threaded (recommended for threads)
./configure --enable-threads
make CPPFLAGS="-DUSE_MARK_SWEEP"
```

Note: `USE_MARK_SWEEP` is not yet integrated into `configure.ac`.  It
must be passed manually via `CPPFLAGS`.


Runtime Options
===============

These emulator command-line flags affect GC behavior:

| Flag | Default | Description |
|------|---------|-------------|
| `--size-heap N` | 128 | Heap size in kilo-refs (1 ref = 8 bytes on 64-bit) |
| `--trace-gc V` | 0 | GC verbosity: 0=quiet, 1=summary, 2=detailed, 3=very detailed |
| `--predump-gc B` | 1 | Run GC before world dump: 0=no, 1=yes |

The default heap is 128 kilo-refs = 128K × 8 bytes = 1 MB on 64-bit
systems.  This is sufficient for most interactive use and compilation
of individual `.oak` files.

The `OAKWORLD` environment variable sets the default world file path.

Example:
```sh
oaklisp --size-heap 4096 --trace-gc 2 --world oakworld.bin -- --load myfile --exit
```


Collector 1: Copying GC (`gc.c`)
================================

The original Oaklisp garbage collector.  This is a semi-space copying
collector: live objects are copied from old space to new space,
compacting the heap in the process.

**How it works:**

1. Allocate a fresh `new_space` of the target size
2. Scan roots (VM registers, stacks, static space) — copy each
   referenced object to new space ("transport")
3. Scavenge new space linearly — for each transported object, scan its
   fields and transport anything still in old space
4. Handle locatives — locatives point to individual cells within
   objects; they are updated after their containing objects are
   transported
5. Process weak pointers — discard entries whose keys were not
   transported
6. Free old space

**Strengths:**
- Simple and well-tested (in use since 1987)
- Compacts the heap — no fragmentation
- Cost is proportional to live data, not heap size
- Automatically expands the heap when live data exceeds 1/3 of space
  (controlled by `RECLAIM_FACTOR = 3`)

**Weaknesses:**
- Stop-the-world — all threads must pause for the entire collection
- Moves all objects — locatives and raw code pointers (`e_pc`) must be
  fixed up, which is complex and incompatible with concurrent operation
- Requires a contiguous destination space at least as large as live data
- Under threads, every allocation acquires `alloc_lock`

**When it runs:**
- Config 1 and 3: Every collection
- Config 2 and 4: Only as a fallback when mark-sweep cannot free enough
  space (heap expansion), and always for pre-dump compaction

**Heap expansion:** When live data exceeds 1/3 of the heap after
collection, the copying GC sets `e_next_newspace_size = 3 * live_data`
so the next collection allocates a larger new space.  An optional
`MAX_NEW_SPACE_SIZE` compile-time cap can limit this growth.


Multi-Threaded Copying GC (Config 3)
-------------------------------------

Under `THREADS` without `USE_MARK_SWEEP`, the copying GC is the only
collector.  Every allocation and every collection contends on global
locks, making this the least scalable configuration for multi-threaded
use.

### Allocation

All allocation goes through the global bump pointer protected by
`alloc_lock`.  The `ALLOCATE_PROT` macro (in `data.h`) works as
follows:

1. Try to acquire `alloc_lock` via `pthread_mutex_trylock`.  While
   waiting, check `gc_pending` — if set, call `wait_for_gc()` to park
   at the GC safepoint (releasing local state first) and resume after
   collection completes.
2. Once the lock is held, check whether `free_point + words` fits
   within `new_space.end`.
3. If space is available, bump `free_point` and release `alloc_lock`.
4. If space is exhausted, trigger a full STW copying collection (see
   below), then bump `free_point` from the fresh heap.  Release
   `alloc_lock`.

Every allocation — even a 3-word cons — acquires and releases the
mutex, which is the main scalability bottleneck of Config 3.

### Collection

When the bump pointer is exhausted, the allocating thread becomes the
GC coordinator:

1. **Signal**: The coordinator sets `gc_pending = true` and acquires
   `gc_lock`.  Other threads notice `gc_pending` during their next
   allocation attempt (in the `pthread_mutex_trylock` spin loop) and
   call `wait_for_gc()`, which sets `gc_ready[thread_index] = 1` and
   blocks on `gc_lock`.
2. **Wait**: The coordinator spins until `gc_ready[i] == 1` for all
   active threads (snapshot of `next_index` at GC start to avoid
   reading uninitialized slots from threads created during GC).
3. **Collect**: With all threads parked, the coordinator runs the
   standard copying GC cycle — transport, scavenge, locative fixup,
   weak pointer processing — scanning roots from **all** threads'
   registers, value stacks, context stacks, and GC examine buffers
   (via the `FORTHREADS` macro).
4. **Resume**: The coordinator clears `gc_ready[my_index]`, sets
   `gc_pending = false`, and releases `gc_lock`.  All parked threads
   unblock from `wait_for_gc()` and resume execution.

The entire collection is stop-the-world: all mutator threads are
paused from step 2 through step 4.  Collection time is proportional
to live data across all threads.


Collector 2: Mark-Sweep GC (`gc-ms.c`)
=======================================

A non-moving mark-sweep collector.  Objects are never relocated, which
means locatives and raw code pointers require no fixup — a prerequisite
for concurrent collection.

Enabled by defining `USE_MARK_SWEEP` at compile time.


Heap Layout
-----------

The mark-sweep GC uses the same contiguous `new_space` as the copying
GC.  Two bitmaps (one bit per `ref_t` word) track state:

- **Objstart bitmap** — marks the first word of every allocated object
- **Mark bitmap** — marks words belonging to reachable objects during
  collection (cleared at the start of each cycle)

These bitmaps are allocated by `ms_init()` at startup and rebuilt by
`ms_reinit()` after any copying GC fallback.


Allocation
----------

Allocation uses a multi-tier strategy:

### Tier 1: Size-class segregated free lists

19 size classes with these maximum sizes (in `ref_t` words):

```
Class:  0   1   2   3   4   5   6    7    8    9
Max:    1   2   3   4   6   8  12   16   24   32

Class: 10  11  12   13   14   15   16    17   18
Max:   48  64  96  128  192  256  512  1024  inf
```

Free list entries occupy the dead object's memory.  `word[0]` is the
next pointer; the entry size is determined by the objstart bitmap (the
distance between consecutive objstart bits).

When a free-list block is larger than the request, the unused gap is
zeroed with `memset` to prevent the copying GC's `scavenge()` from
seeing stale pointer values.

### Tier 2: Bump pointer

When free lists are empty, allocation advances `free_point` into
virgin heap space.  An objstart bit is set for the new object.

### Tier 3: Lazy sweep (after a collection)

If free lists are empty and the bump pointer is exhausted, but a
collection has recently completed, `ms_lazy_sweep_one()` sweeps one
4096-word block of the heap to populate free lists.  This is tried
before triggering a new collection.

### Tier 4: Collection

If all of the above fail, `ms_collect()` runs a mark-sweep cycle.

### Tier 5: Copying GC fallback

If mark-sweep still cannot free enough space, the copying GC is invoked
to compact live data and expand the heap.  Afterward, `ms_reinit()`
rebuilds the bitmaps from scratch.


Thread-Local Allocation Buffers (TLABs)
---------------------------------------

Active only under `THREADS + USE_MARK_SWEEP`.

Each thread has a private bump-pointer region (TLAB) carved from the
global heap.  TLAB size: 4096 words (`TLAB_SIZE`).

**Fast path (no lock):**
1. Check if the TLAB has enough space
2. Bump `tlab_cursor_array[thread_index]`
3. Set objstart bit atomically (`ms_bump_alloc_notify_atomic`)
4. If a concurrent GC is active, set the mark bit ("born-black")

**Slow path (acquires `alloc_lock`):**
1. Try free list (`ms_free_list_alloc`)
2. Try lazy sweep + free list
3. Try TLAB refill from global bump pointer
4. Try global bump pointer (for large allocations)
5. If all fail: trigger `ms_collect()`

TLABs are retired before every STW pause (`ms_tlab_retire_all`):
unused TLAB space is zeroed and given an objstart bit so sweep can
reclaim it.


Collection: Single-Threaded Mode
---------------------------------

Under `USE_MARK_SWEEP` without `THREADS`, collection is stop-the-world:

1. Clear mark bitmap
2. Mark roots (VM registers, stacks, static space)
3. Trace: iteratively follow marked objects' pointer fields,
   marking referenced objects (using an explicit mark stack)
4. Process weak pointers (discard unmarked entries)
5. Sweep: scan the heap linearly using the objstart bitmap;
   any object whose first word is not marked is added to the
   appropriate size-class free list
6. If free space is still insufficient, fall back to copying GC


Collection: Multi-Threaded Mode (Concurrent)
---------------------------------------------

Under `THREADS + USE_MARK_SWEEP`, the GC uses a three-phase concurrent
collection with two brief STW pauses:

### Phase 1: Initial Mark (STW)

The GC coordinator thread (whichever thread triggered collection):
1. Sets `gc_pending` — all other threads park via `wait_for_gc()`
2. Scans roots for all threads (`ms_mark_roots()`)
3. Activates SATB write barriers (`gc_satb_active = 1`)
4. Releases all threads — mutators resume

### Phase 2: Concurrent Mark

The GC coordinator traces the heap while mutators run:
- Uses `ms_trace_concurrent()` with atomic mark-bitmap operations
  (`__atomic_fetch_or`)
- Mutators' SATB barriers log old pointer values before any heap write
- Objects allocated from free lists during this phase are marked
  atomically ("born-black") via `ms_mark_alloc()`
- Objects allocated via bump pointer / TLABs are above `sweep_limit`
  and are implicitly live (not swept)

### Phase 3: Remark (STW)

The GC coordinator parks all threads again, then:
1. Retires TLABs allocated during concurrent mark
2. Drains all SATB buffers into the mark stack
   (`satb_drain_to_mark_stack()`)
3. Re-scans roots (stacks are C arrays, not covered by SATB)
4. Scans objects allocated above `sweep_limit` (may contain pointers
   to old objects)
5. Traces all newly discovered references
6. Deactivates SATB barriers
7. Prepares lazy sweep (clears free lists, sets sweep cursor)
8. Eagerly sweeps enough blocks to satisfy the immediate allocation
9. Releases all threads

### Post-Remark: Lazy Sweep

After threads are released, sweep runs on demand:
- Each call to `ms_lazy_sweep_one()` sweeps one 4096-word block
- Called from `ms_alloc_slow_locked()` when free lists are empty
- Naturally adapts sweep rate to allocation pressure
- `ms_lazy_sweep_finish()` flushes remaining blocks at the start of
  the next GC cycle

### GC Phase Tracking

The `gc_phase` variable tracks the current state:

```
GC_PHASE_IDLE
  → GC_PHASE_INITIAL_MARK   (STW pause 1)
  → GC_PHASE_CONCURRENT_MARK (mutators running)
  → GC_PHASE_REMARK          (STW pause 2)
  → GC_PHASE_SWEEP           (lazy sweep, mutators running)
  → GC_PHASE_IDLE
```


SATB Write Barriers
--------------------

Snapshot-at-the-beginning (SATB) barriers ensure concurrent marking
correctness.  They are inserted at all 10 heap-write sites in the
bytecode interpreter (`loop.c`):

1. SET-CONTENTS (locative store)
2. STORE-BP-I (indexed base-pointer store)
3. SET-CAR (cons car mutation)
4. SET-CDR (cons cdr mutation)
5. FILL-CONTINUATION (continuation segments)
6. TEST-AND-SET-LOCATIVE (atomic CAS)
7. STORE-BP n (base-pointer store variant)
8. STORE-ENV n (environment store)
9. STORE-SLOT (general slot store)
10. Operation cache writes (method/type cache)

When `gc_satb_active == 0` (the normal state), the barrier is a single
well-predicted branch with negligible overhead.  When active, the old
pointer value is logged to a per-thread buffer (1024 entries).

Fresh allocations (CONS, MAKE-CELL, ALLOCATE, etc.) are not barriered
because they write to newly created objects, not overwrite existing
pointer fields.


Interaction Between the Two Collectors
======================================

When `USE_MARK_SWEEP` is enabled, the two collectors have distinct
roles:

| Role | Collector |
|------|-----------|
| Normal collection | Mark-sweep |
| Heap expansion | Copying GC (expands and compacts) |
| Pre-dump compaction | Copying GC (produces contiguous heap for serialization) |
| Bitmap rebuild after expansion | `ms_reinit()` |

The copying GC is always available.  During a copying GC fallback:
1. `ms_collect()` sets `original_newspace_size = 3 * live_data` so the
   copying GC allocates a large enough destination space
2. `gc()` runs its normal transport/scavenge cycle
3. During transport, `ms_record_transport(new_place)` records each
   destination address in a deferred buffer
4. After copying completes, `ms_reinit()` allocates fresh bitmaps and
   replays the transport buffer to set objstart bits for all surviving
   objects

Without this replay mechanism, objects surviving a copying GC fallback
would have no objstart bits and be invisible to subsequent mark-sweep
collections.


Allocation Flow Summary
========================

```
                    USE_MARK_SWEEP + THREADS
                    ========================

    ┌─ TLAB has space? ─── yes ──→ bump TLAB cursor (no lock)
    │                               set objstart bit (atomic)
    │                               if concurrent GC: born-black mark
    │                               DONE
    │
    no
    │
    ├─ acquire alloc_lock
    │
    ├─ free list? ─── yes ──→ return block, DONE
    │
    ├─ lazy sweep → free list? ─── yes ──→ return block, DONE
    │
    ├─ TLAB refill from bump pointer? ─── yes ──→ bump TLAB, DONE
    │
    ├─ global bump pointer? ─── yes ──→ return block, DONE
    │
    ├─ ms_collect() (concurrent mark + lazy sweep)
    │
    ├─ retry free list / lazy sweep / bump? ─── yes ──→ DONE
    │
    ├─ gc() (copying GC: compact + expand heap)
    │   ms_reinit() (rebuild bitmaps)
    │
    └─ bump pointer (guaranteed space after expansion) ──→ DONE


                    USE_MARK_SWEEP (single-threaded)
                    ================================

    ┌─ free list? ─── yes ──→ DONE
    │
    ├─ lazy sweep → free list? ─── yes ──→ DONE
    │
    ├─ bump pointer has space? ─── yes ──→ DONE
    │
    ├─ ms_collect() (STW mark + full sweep)
    │
    ├─ retry free list / lazy sweep / bump? ─── yes ──→ DONE
    │
    ├─ gc() (copying GC: compact + expand)
    │   ms_reinit()
    │
    └─ bump pointer ──→ DONE


                    THREADS (no USE_MARK_SWEEP)
                    ===========================

    ┌─ acquire alloc_lock
    │   (while waiting, check gc_pending → wait_for_gc() if set)
    │
    ├─ bump pointer has space? ─── yes ──→ release lock, DONE
    │
    ├─ gc() (STW copying GC — all threads parked)
    │
    └─ bump pointer ──→ release lock, DONE


                    Default (single-threaded, no USE_MARK_SWEEP)
                    =============================================

    ┌─ bump pointer has space? ─── yes ──→ DONE
    │
    ├─ gc() (copying GC)
    │
    └─ bump pointer ──→ DONE
```


GC Trace Output
===============

With `--trace-gc 1`, the GC prints a one-line summary per collection:

```
;MS-GC:96%         mark-sweep, 96% of used space reclaimed
;GC:45%            copying GC, 45% reclaimed
;GC:45%,resize:N   copying GC expanded heap to N refs
```

With `--trace-gc 2`:

```
; Mark-sweep GC due to space crunch in CONS instruction.
; Marking... [heap=262144, used=260000, free_list=0] roots
; Mark complete.
; Sweeping... 240000 words reclaimed.
; MS-GC complete.  20000 used, 242144 free (240000 list + 2144 bump).
```

Under `THREADS + USE_MARK_SWEEP` with `--trace-gc 2`:

```
; Mark-sweep GC due to space crunch in CONS instruction.
; Marking... [heap=262144, used=260000, free_list=0]
; STW initial mark... roots
; Concurrent tracing... done.
; STW remark... done.
; Mark complete.
; Lazy sweep prepared (260000 words to sweep).
; Eagerly swept 3576 words.
; MS-GC complete.  3576 free (3575 list + 1 bump).
```


Key Design Decisions
====================

**Non-moving mark-sweep as foundation for concurrency.**  The copying
GC moves every object, requiring all pointers (including locatives and
the raw instruction pointer `e_pc`) to be updated.  This is
fundamentally incompatible with concurrent mutation.  The mark-sweep
GC never moves objects, so no pointer fixup is needed, and mutator
threads can safely dereference pointers during collection.

**Sweep uses objstart bitmap, not type-pointer length.**  The
`MAKE-CELL` instruction allocates 1-word cells where `word[0]` stores
an arbitrary value, not a type pointer.  Following it as a type pointer
gives incorrect object lengths.  The objstart bitmap is the
authoritative source of object boundaries.

**Free-list gap zeroing.**  When a free-list block is larger than the
request, the gap is zeroed.  This prevents the copying GC's linear
`scavenge()` from seeing stale `PTR_TAG` values in the gap during a
fallback collection.

**Lazy sweep.**  Rather than sweeping the entire heap in a single
STW pass after marking, sweep runs incrementally during allocation.
This eliminates the STW sweep pause entirely under `THREADS` and
amortizes sweep cost across allocations in single-threaded mode.

**Born-black allocation.**  Objects allocated from free lists during
concurrent mark are below `sweep_limit` and would be incorrectly
freed.  Setting their mark bits atomically prevents this.  Objects
allocated via bump pointer or TLAB are above `sweep_limit` and are
never swept (implicitly live).

**Copying GC retained as fallback.**  Rather than implementing heap
compaction within the mark-sweep framework, the proven copying GC
handles compaction and expansion.  This keeps the mark-sweep code
simpler and ensures world dumps always produce compact, serializable
heaps.


Source Files
============

| File | Purpose |
|------|---------|
| `gc-ms.c` | Mark-sweep GC: bitmaps, free lists, mark, sweep, TLABs, SATB, concurrent collection |
| `gc-ms.h` | Mark-sweep GC declarations, constants, SATB barrier macro |
| `gc.c` | Copying GC: transport, scavenge, locative fixup, heap expansion |
| `gc.h` | Copying GC declarations, `GC_MEMORY`/`GC_RECALL` macros |
| `data.h` | `ALLOCATE_PROT` macro (three variants), heap data structures |
| `loop.c` | SATB barrier insertion sites, allocation instruction handlers |
| `oaklisp.c` | GC initialization (`ms_init`, `gc_safepoint_init`) |
| `weak.c` | Weak pointer table processing (used by both collectors) |


Tuning Constants
================

These are compile-time constants in the header files:

| Constant | Value | File | Description |
|----------|-------|------|-------------|
| `DEFAULT_NEWSPACE` | 128 | `data.h` | Default heap size in kilo-refs |
| `RECLAIM_FACTOR` | 3 | `gc.c` | Copying GC target: live data should be < 1/3 of heap |
| `NUM_SIZE_CLASSES` | 19 | `gc-ms.h` | Number of segregated free list size classes |
| `TLAB_SIZE` | 4096 | `gc-ms.h` | Per-thread allocation buffer size in words |
| `SWEEP_BLOCK_SIZE` | 4096 | `gc-ms.h` | Lazy sweep granularity in words |
| `SATB_BUFFER_SIZE` | 1024 | `gc-ms.h` | SATB write barrier buffer entries per thread |
| `MAX_NEW_SPACE_SIZE` | (optional) | `Makefile.am` | Hard cap on heap size (disabled by default) |

To limit maximum heap growth, compile with:
```sh
make CPPFLAGS="-DUSE_MARK_SWEEP -DMAX_NEW_SPACE_SIZE=16000000"
```


Recommendations
===============

**For single-threaded use:**  The default build (Config 1, copying GC)
is simple and well-tested.  Config 2 (`USE_MARK_SWEEP` alone) offers
comparable performance and is useful for testing the mark-sweep code
without threading complexity.

**For multi-threaded use:**  Config 4 (`THREADS + USE_MARK_SWEEP`) is
strongly recommended.  The concurrent mark-sweep collector minimizes
STW pause times (only initial mark and remark are STW; sweep is lazy),
and TLABs eliminate allocation lock contention for the common case.
Config 3 (`THREADS` alone) works but has longer STW pauses (full
copying GC) and all allocations contend on `alloc_lock`.

**Heap sizing:**  The default 128K refs (1 MB) is adequate for
interactive use.  For compiling large files or running allocation-heavy
programs, 1024-4096K (8-32 MB) is recommended.  The GC automatically
expands the heap when needed, but starting larger avoids unnecessary
early collections.
