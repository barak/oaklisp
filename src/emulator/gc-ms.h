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
 * gc-ms.h - Non-moving mark-sweep garbage collector
 *
 * This is a non-moving GC designed to eventually support concurrent
 * operation.  Objects are never relocated, which means locatives and
 * raw code pointers (e_pc) require no special handling.
 *
 * The heap uses the same contiguous new_space as the copying GC.
 * Bitmaps track object starts and mark status.  Allocation uses
 * size-class segregated free lists with bump-pointer fallback.
 *
 * The copying GC (gc.c) is retained for world-dump compaction.
 */

#ifndef GC_MS_H_INCLUDED
#define GC_MS_H_INCLUDED

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "oak-atomic.h"

/* ref_t is defined in data.h; since gc-ms.h is included from data.h
   after the typedef, we just need the name visible here. */
#ifndef _DATA_H_INCLUDED
typedef size_t ref_t;
#endif

/* Number of size classes.  Classes 0..NUM_SIZE_CLASSES-2 have bounded
   sizes; the last class is for objects larger than the second-to-last
   class maximum. */
#define NUM_SIZE_CLASSES 19

/* Default TLAB size in ref_t words.  Each thread carves chunks of
   this size from the global bump pointer for lock-free allocation. */
#define TLAB_SIZE 4096

/* Initialize bitmaps and free lists for the current new_space.
   Must be called after new_space is allocated. */
void ms_init(void);

/* Tear down bitmaps and free lists. */
void ms_destroy(void);

/* Re-initialize after the heap is replaced (e.g., load-world). */
void ms_reinit(void);

/* Try to allocate n_words from free lists.  Returns pointer to the
   first word of a region of at least n_words, or NULL if no free list
   entry is available.  Does NOT fall back to bump pointer. */
ref_t *ms_free_list_alloc(size_t n_words);

/* Notify the allocator that bump-pointer allocation just occurred
   at address p for n_words.  Sets the object-start bit. */
void ms_bump_alloc_notify(ref_t *p, size_t n_words);

/* Perform stop-the-world mark-sweep collection.
   pre_dump: if true, delegate to copying GC for compaction.
   reason: human-readable string for trace output.
   amount: minimum free words needed after collection. */
void ms_collect(bool pre_dump, char *reason, size_t amount);

/* Return the size class index for an allocation of n_words. */
int ms_size_class(size_t n_words);

/* Return the maximum object size (words) for a given size class. */
size_t ms_class_max_words(int sc);

/* Check whether an address is within the managed heap (new_space). */
bool ms_is_heap_ptr(const ref_t *p);

#ifdef THREADS
/* Per-thread TLAB (thread-local allocation buffer) state arrays.
   tlab_cursor_array[i] points to the next free word in thread i's TLAB.
   tlab_end_array[i] points one past the last word.  Both are NULL when
   the TLAB is exhausted or not yet allocated. */
extern ref_t *tlab_cursor_array[];
extern ref_t *tlab_end_array[];

/* Slow-path allocator called with alloc_lock held.  Tries free list,
   TLAB refill, and global bump pointer.  Returns NULL if GC is needed.
   Updates *tlab_cursor_p / *tlab_end_p on TLAB refill. */
ref_t *ms_alloc_slow_locked(size_t n_words,
			    ref_t **tlab_cursor_p, ref_t **tlab_end_p);

/* Retire a single TLAB's unused portion so it can be swept. */
void ms_tlab_retire(ref_t *cursor, ref_t *end);

/* Retire all threads' TLABs.  Called before STW collection. */
void ms_tlab_retire_all(void);
#endif

/* Atomic objstart-bit set for lock-free TLAB allocation. */
void ms_bump_alloc_notify_atomic(ref_t *p, size_t n_words);

/* Record a transport destination during copying GC so that ms_reinit
   can rebuild objstart bits for the compacted heap. */
void ms_record_transport(ref_t *new_place);


/* ------------------------------------------------------------------ */
/*  SATB write barrier                                                 */
/* ------------------------------------------------------------------ */

/* When gc_satb_active is nonzero, the SATB_BARRIER macro logs the old
   value of a heap slot before it is overwritten.  This is the
   snapshot-at-the-beginning invariant needed for concurrent marking.

   Under THREADS + USE_MARK_SWEEP, gc_satb_active is set to 1 during
   the concurrent mark phase and cleared during the remark STW pause.
   In single-threaded mode it is always 0.  The compiler should predict
   the branch as not-taken (negligible overhead). */

extern int gc_satb_active;

#define SATB_BUFFER_SIZE 1024

#ifdef THREADS
extern ref_t  satb_buffer_array[][SATB_BUFFER_SIZE];
extern size_t satb_count_array[];
#else
extern ref_t  satb_buffer[];
extern size_t satb_count;
#endif

/* Log the old value at *addr to the per-thread SATB buffer.
   Called from the SATB_BARRIER macro; do not call directly. */
void satb_log_old(ref_t *addr);

/* Drain all SATB buffers.  In Phase 4 this will feed entries to the
   concurrent marker; for now it just clears the buffers. */
void satb_flush_all(void);

/* Barrier macro: insert before any heap write that overwrites an
   existing pointer field.  No-op when gc_satb_active == 0.
   Overrides the no-op fallback defined in data.h. */
#undef SATB_BARRIER
#define SATB_BARRIER(addr) \
    do { if (OAK_UNLIKELY(gc_satb_active)) \
	     satb_log_old(addr); } while (0)


/* ------------------------------------------------------------------ */
/*  Concurrent marking (Phase 4)                                       */
/* ------------------------------------------------------------------ */

/* GC phase tracking.  Controls born-black allocation during
   concurrent mark and determines whether atomic bitmap operations
   are needed. */
enum gc_phase_t {
    GC_PHASE_IDLE = 0,
    GC_PHASE_INITIAL_MARK,
    GC_PHASE_CONCURRENT_MARK,
    GC_PHASE_REMARK,
    GC_PHASE_SWEEP
};
extern volatile int gc_phase;

#ifdef THREADS
/* True while a concurrent mark-sweep cycle is in progress.
   Threads that exhaust the heap wait instead of starting a second
   collection.  Also checked by gc() to skip its own thread
   synchronization when called as a fallback from ms_collect. */
extern volatile int gc_concurrent_in_progress;

/* Initialize concurrent GC synchronization (called once at startup). */
void gc_safepoint_init(void);

/* Wait for an in-progress concurrent GC to complete.
   Called from ms_collect when a second thread needs collection. */
void ms_wait_concurrent_gc(void);
#endif

/* Drain all SATB buffers into the GC mark stack so that the
   concurrent marker can trace them.  Called during remark. */
void satb_drain_to_mark_stack(void);

/* Set mark bit for a newly allocated object during concurrent mark
   (born-black).  Safe to call at any time; no-op if address is
   outside the heap or if no collection is in progress. */
void ms_mark_alloc(ref_t *p);


/* ------------------------------------------------------------------ */
/*  Lazy sweep (Phase 5)                                               */
/* ------------------------------------------------------------------ */

/* Block size for lazy sweep: the sweep cursor advances by this many
   words per ms_lazy_sweep_one() call. */
#define SWEEP_BLOCK_SIZE 4096

/* Prepare for lazy sweep after the mark phase completes.
   Clears free lists and resets the sweep cursor. */
void ms_prepare_lazy_sweep(void);

/* Sweep one block (up to SWEEP_BLOCK_SIZE words) from the sweep cursor.
   Returns true if a block was swept (free lists may now have entries),
   false if the entire heap has been swept.  Thread-safe when called
   under alloc_lock. */
bool ms_lazy_sweep_one(void);

/* Sweep all remaining unsswept blocks.  Called at the start of a new
   GC cycle to flush any residual from the previous cycle. */
void ms_lazy_sweep_finish(void);

#endif
