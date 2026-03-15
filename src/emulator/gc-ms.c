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
 * gc-ms.c - Non-moving mark-sweep garbage collector
 *
 * Design:
 *   - Heap is the existing contiguous new_space.
 *   - Two bitmaps: object-start (1 bit per ref_t word) and mark.
 *   - Allocation: segregated free lists by size class, with
 *     bump-pointer (free_point) fallback for virgin space.
 *   - Collection: stop-the-world mark from roots, sweep to rebuild
 *     free lists.
 *   - The copying GC (gc.c) is retained for pre-dump compaction.
 */

#define _REENTRANT

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "data.h"
#include "gc-ms.h"
#include "gc.h"
#include "weak.h"
#include "xmalloc.h"
#include "stacks.h"
#include "signals.h"
#include "oak-atomic.h"


/* ------------------------------------------------------------------ */
/*  Bitmaps                                                            */
/* ------------------------------------------------------------------ */

static uint64_t *objstart_bitmap;   /* 1 bit per ref_t word in new_space */
static uint64_t *mark_bitmap;       /* 1 bit per ref_t word in new_space */
static size_t bitmap_words;         /* number of uint64_t in each bitmap */

#define BM_IDX(p)    ((size_t)((p) - new_space.start))
#define BM_SET(bm, i)   ((bm)[(i)/64] |=  (1ULL << ((i) % 64)))
#define BM_CLR(bm, i)   ((bm)[(i)/64] &= ~(1ULL << ((i) % 64)))
#define BM_TEST(bm, i)  ((bm)[(i)/64] &   (1ULL << ((i) % 64)))


/* ------------------------------------------------------------------ */
/*  Size classes                                                       */
/* ------------------------------------------------------------------ */

/*
 * Class   Max words
 *   0       1
 *   1       2
 *   2       3    (cons pairs)
 *   3       4
 *   4       6
 *   5       8    (lambda = 8 words)
 *   6      12
 *   7      16
 *   8      24
 *   9      32
 *  10      48
 *  11      64
 *  12      96
 *  13     128
 *  14     192
 *  15     256
 *  16     512
 *  17    1024
 *  18    inf   (large objects)
 */

static const size_t class_max[NUM_SIZE_CLASSES] = {
    1, 2, 3, 4, 6, 8, 12, 16, 24, 32,
    48, 64, 96, 128, 192, 256, 512, 1024,
    (size_t)-1   /* large: no upper bound */
};

int
ms_size_class(size_t n)
{
    /* Find smallest class whose max >= n.
       Linear scan is fine for 19 classes. */
    for (int i = 0; i < NUM_SIZE_CLASSES; i++)
	if (n <= class_max[i])
	    return i;
    return NUM_SIZE_CLASSES - 1;
}

size_t
ms_class_max_words(int sc)
{
    return class_max[sc];
}


/* ------------------------------------------------------------------ */
/*  Free lists                                                         */
/* ------------------------------------------------------------------ */

/*
 * Each free-list entry occupies the dead object's memory:
 *   word[0] = next pointer (ref_t* cast to ref_t), 0 = end of list
 *   The entry size is the original object's size (known from objstart bitmap).
 *
 * free_list_heads[sc] points to the first entry in class sc, or NULL.
 */

static ref_t *free_list_heads[NUM_SIZE_CLASSES];

/* Total words on free lists (for statistics / GC triggering). */
static size_t free_list_total_words;


ref_t *
ms_free_list_alloc(size_t n_words)
{
    int sc = ms_size_class(n_words);

    /* Only try the exact class — don't search larger classes.
       Within the class, only return entries with actual_size >= n_words. */
    {
	ref_t **prev = &free_list_heads[sc];
	ref_t *p = *prev;
	while (p) {
	    size_t actual_size = (sc == 0) ? 1 : (size_t)p[1];
	    /* Validate actual_size to guard against corrupt free list entries. */
	    if (actual_size == 0 || actual_size > new_space.size ||
		p + actual_size > new_space.end) {
		prev = (ref_t **)p;
		p = (ref_t *)(*p);
		continue;
	    }
	    if (actual_size >= n_words) {
		*prev = (ref_t *)(*p);
		BM_SET(objstart_bitmap, BM_IDX(p));
		free_list_total_words -= actual_size;

		/* Born-black: during concurrent mark, objects allocated
		   from the free list (below sweep_limit) must have their
		   mark bit set so sweep doesn't reclaim them. */
		if (OAK_UNLIKELY(
			gc_phase >= GC_PHASE_CONCURRENT_MARK))
		    ms_mark_alloc(p);

		/* Zero gap words beyond what the caller will use.
		   This prevents the copying GC's linear scavenge
		   from seeing stale PTR_TAG values in the gap,
		   and ensures no stale data leaks into live objects
		   via the sweep's length computation. */
		if (actual_size > n_words)
		    memset(p + n_words, 0,
			   (actual_size - n_words) * sizeof(ref_t));
		return p;
	    }
	    prev = (ref_t **)p;
	    p = (ref_t *)(*p);
	}
    }
    return NULL;
}


void
ms_bump_alloc_notify(ref_t *p, size_t n_words)
{
    BM_SET(objstart_bitmap, BM_IDX(p));
    (void)n_words;
}


void
ms_bump_alloc_notify_atomic(ref_t *p, size_t n_words)
{
    size_t idx = BM_IDX(p);
    OAK_ATOMIC_FETCH_OR_U64(&objstart_bitmap[idx / 64],
			    1ULL << (idx % 64));
    (void)n_words;
}


/* ------------------------------------------------------------------ */
/*  TLABs (thread-local allocation buffers)                            */
/* ------------------------------------------------------------------ */

#ifdef THREADS
#include "threads.h"

ref_t *tlab_cursor_array[MAX_THREAD_COUNT];
ref_t *tlab_end_array[MAX_THREAD_COUNT];

/*
 * Carve a TLAB from the global bump pointer.
 * Must be called with alloc_lock held.
 * Returns the start of the new TLAB, or NULL if insufficient space.
 * Sets *out_end to one past the last word of the TLAB.
 */
static ref_t *
ms_tlab_refill(size_t min_words, ref_t **out_end)
{
    size_t tlab_words = TLAB_SIZE;
    if (tlab_words < min_words)
	tlab_words = min_words;

    if (free_point + tlab_words >= new_space.end)
	return NULL;

    ref_t *start = free_point;
    free_point += tlab_words;
    *out_end = start + tlab_words;
    return start;
}


ref_t *
ms_alloc_slow_locked(size_t n_words,
		     ref_t **tlab_cursor_p, ref_t **tlab_end_p)
{
    ref_t *p;

    /* 1. Try free list. */
    p = ms_free_list_alloc(n_words);
    if (p)
	return p;

    /* 1b. Try lazy sweep to populate free lists. */
    while (ms_lazy_sweep_one()) {
	p = ms_free_list_alloc(n_words);
	if (p)
	    return p;
    }

    /* 2. Try TLAB refill for small allocations. */
    if (n_words <= TLAB_SIZE) {
	ref_t *new_end;
	ref_t *new_start = ms_tlab_refill(n_words, &new_end);
	if (new_start) {
	    *tlab_cursor_p = new_start + n_words;
	    *tlab_end_p = new_end;
	    ms_bump_alloc_notify(new_start, n_words);
	    return new_start;
	}
    }

    /* 3. Try global bump pointer (large allocs or TLAB refill failed). */
    if (free_point + n_words < new_space.end) {
	p = free_point;
	free_point += n_words;
	ms_bump_alloc_notify(p, n_words);
	return p;
    }

    return NULL;  /* GC needed */
}


void
ms_tlab_retire(ref_t *cursor, ref_t *end)
{
    if (!cursor || cursor >= end)
	return;

    size_t remaining = (size_t)(end - cursor);

    /* Zero the unused region so it doesn't confuse the mark phase
       (zeroed words are fixnum 0, not pointers). */
    memset(cursor, 0, remaining * sizeof(ref_t));

    /* Set an objstart bit so the sweep can find and reclaim it. */
    BM_SET(objstart_bitmap, BM_IDX(cursor));
}


void
ms_tlab_retire_all(void)
{
    extern int next_index;
    int n = next_index;   /* snapshot under alloc_lock */

    for (int i = 0; i < n; i++) {
	ms_tlab_retire(tlab_cursor_array[i], tlab_end_array[i]);
	tlab_cursor_array[i] = NULL;
	tlab_end_array[i] = NULL;
    }
}
#endif /* THREADS */


/* ------------------------------------------------------------------ */
/*  SATB write barrier buffers                                         */
/* ------------------------------------------------------------------ */

int gc_satb_active = 0;

#ifdef THREADS
ref_t  satb_buffer_array[MAX_THREAD_COUNT][SATB_BUFFER_SIZE];
size_t satb_count_array[MAX_THREAD_COUNT];
#else
ref_t  satb_buffer[SATB_BUFFER_SIZE];
size_t satb_count = 0;
#endif

void
satb_log_old(ref_t *addr)
{
    ref_t old_val = *addr;

    /* Only log pointer-tagged values (skip fixnums and immediates). */
    if (!(old_val & PTR_MASK))
	return;

#ifdef THREADS
    int idx = *(int *)oak_tls_get(index_key);
    if (satb_count_array[idx] < SATB_BUFFER_SIZE)
	satb_buffer_array[idx][satb_count_array[idx]++] = old_val;
    /* If full, silently drop.  Phase 4 will add proper flushing. */
#else
    if (satb_count < SATB_BUFFER_SIZE)
	satb_buffer[satb_count++] = old_val;
#endif
}

void
satb_flush_all(void)
{
#ifdef THREADS
    extern int next_index;
    for (int i = 0; i < next_index; i++)
	satb_count_array[i] = 0;
#else
    satb_count = 0;
#endif
}


/* ------------------------------------------------------------------ */
/*  Concurrent marking state (Phase 4)                                 */
/* ------------------------------------------------------------------ */

volatile int gc_phase = GC_PHASE_IDLE;

/* Sweep limit: only sweep objects that existed before the mark phase
   started.  Objects allocated during concurrent mark (above this
   address) are implicitly live and will not be swept. */
static ref_t *sweep_limit;

#ifdef THREADS
volatile int gc_concurrent_in_progress = 0;

static oak_mutex_t gc_concurrent_lock = OAK_MUTEX_INITIALIZER;
static oak_cond_t  gc_concurrent_done_cv = OAK_COND_INITIALIZER;

void
gc_safepoint_init(void)
{
#ifdef OAK_NEEDS_DYNAMIC_MUTEX_INIT
    oak_mutex_init(&gc_concurrent_lock);
    oak_cond_init(&gc_concurrent_done_cv);
#endif
}

void
ms_wait_concurrent_gc(void)
{
    oak_mutex_lock(&gc_concurrent_lock);
    while (gc_concurrent_in_progress)
	oak_cond_wait(&gc_concurrent_done_cv, &gc_concurrent_lock);
    oak_mutex_unlock(&gc_concurrent_lock);
}

/* Request all mutator threads to stop (STW pause).
   Sets gc_pending and waits for all gc_ready[] flags.
   Returns the thread count snapshot. */
static int
stw_request_pause(int my_index_val)
{
    int gc_thread_count;

    gc_ready[my_index_val] = 1;
    set_gc_flag(true);   /* locks gc_lock, sets gc_pending */
    gc_thread_count = next_index;

    /* Busy-wait for all threads to reach a safepoint. */
    {
	bool ready = false;
	while (!ready) {
	    ready = true;
	    for (int i = 0; i < gc_thread_count; i++) {
		if (gc_ready[i] == 0) {
		    ready = false;
		    oak_thread_yield();
		    break;
		}
	    }
	}
    }

    return gc_thread_count;
}

/* Release all mutator threads from a STW pause. */
static void
stw_release(int my_index_val)
{
    gc_ready[my_index_val] = 0;
    set_gc_flag(false);   /* gc_pending=false, unlocks gc_lock */
}

#endif /* THREADS */


/* Set mark bit for born-black allocation during concurrent mark. */
void
ms_mark_alloc(ref_t *p)
{
    if (!mark_bitmap)
	return;
    if (p >= new_space.start && p < new_space.end) {
	size_t idx = BM_IDX(p);
#ifdef THREADS
	OAK_ATOMIC_FETCH_OR_U64(&mark_bitmap[idx / 64],
				1ULL << (idx % 64));
#else
	BM_SET(mark_bitmap, idx);
#endif
    }
}


/* ------------------------------------------------------------------ */
/*  Deferred objstart recording for copying GC fallback                */
/* ------------------------------------------------------------------ */

static ref_t **deferred_objstarts;
static size_t  deferred_count;
static size_t  deferred_cap;

void
ms_record_transport(ref_t *new_place)
{
    if (deferred_count >= deferred_cap) {
	deferred_cap = deferred_cap ? deferred_cap * 2 : 4096;
	deferred_objstarts = (ref_t **)realloc(
	    deferred_objstarts, deferred_cap * sizeof(ref_t *));
	if (!deferred_objstarts) {
	    fprintf(stderr, "gc-ms: deferred objstart realloc failed\n");
	    exit(EXIT_FAILURE);
	}
    }
    deferred_objstarts[deferred_count++] = new_place;
}


/* ------------------------------------------------------------------ */
/*  Object length (for sweep — no forwarding pointers to follow)       */
/* ------------------------------------------------------------------ */

static size_t
ms_get_length(ref_t *obj)
{
    ref_t typ_ref = obj[0];
    if (!TAG_IS(typ_ref, PTR_TAG))
	return 1;   /* bare cell or corrupt — treat as 1 word */

    ref_t *typ = REF_TO_PTR(typ_ref);

    /* The type object itself must be in a valid heap region. */
    if (!SPATIC_PTR(typ) && !NEW_PTR(typ))
	return 1;   /* type pointer is outside known heaps */

    ref_t vlen_p = typ[TYPE_VAR_LEN_P_OFF];

    if (vlen_p == e_nil || vlen_p == 0) {
	/* Fixed-length type. */
	size_t len = (size_t)REF_TO_INT(typ[TYPE_LEN_OFF]);
	if (len == 0 || len > new_space.size)
	    return 1;
	return len;
    } else {
	/* Variable-length: length is in slot 1. */
	size_t len = (size_t)REF_TO_INT(obj[1]);
	if (len == 0 || len > new_space.size)
	    return 1;
	return len;
    }
}


/* ------------------------------------------------------------------ */
/*  Find object start from an interior address (for locatives)         */
/* ------------------------------------------------------------------ */

static ref_t *
find_object_start(ref_t *addr)
{
    if (!ms_is_heap_ptr(addr))
	return NULL;

    size_t idx = BM_IDX(addr);
    size_t w = idx / 64;
    int bit = (int)(idx % 64);

    /* Mask: keep bits 0..bit (at or below the target). */
    uint64_t word = objstart_bitmap[w] & ((2ULL << bit) - 1);

    /* Scan backward through bitmap words. */
    while (word == 0) {
	if (w == 0) return NULL;
	w--;
	word = objstart_bitmap[w];
    }

    int highest = 63 - CLZ64(word);
    return new_space.start + w * 64 + highest;
}


/* ------------------------------------------------------------------ */
/*  Mark phase                                                         */
/* ------------------------------------------------------------------ */

/* Dynamically growing mark stack. */
static ref_t **mark_stack;
static size_t mark_stack_size;
static size_t mark_stack_top;

#define MARK_STACK_INITIAL 4096

static void
mark_stack_init(void)
{
    mark_stack_size = MARK_STACK_INITIAL;
    mark_stack_top = 0;
    mark_stack = (ref_t **)xmalloc(mark_stack_size * sizeof(ref_t *));
}

static void
mark_stack_push(ref_t *obj)
{
    if (mark_stack_top >= mark_stack_size) {
	mark_stack_size *= 2;
	mark_stack = (ref_t **)realloc(mark_stack,
				       mark_stack_size * sizeof(ref_t *));
	if (!mark_stack) {
	    fprintf(stderr, "gc-ms: mark stack realloc failed\n");
	    exit(EXIT_FAILURE);
	}
    }
    mark_stack[mark_stack_top++] = obj;
}

static ref_t *
mark_stack_pop(void)
{
    return mark_stack[--mark_stack_top];
}


/*
 * Mark an object in new_space and push it onto the mark stack
 * if not already marked.  For spatic objects, do nothing (they
 * are permanent and always considered live).
 */
static void
ms_mark_ref(ref_t r)
{
    if (!(r & PTR_MASK))
	return;   /* fixnum or immediate — not a pointer */

    ref_t *p = ANY_TO_PTR(r);

    if (TAG_IS(r, LOC_TAG)) {
	/* Locative: find the containing object and mark that. */
	if (!NEW_PTR(p))
	    return;   /* spatic or outside heap */
	p = find_object_start(p);
	if (!p) return;
    } else {
	/* PTR_TAG reference. */
	if (!NEW_PTR(p))
	    return;   /* spatic — always live */
    }

    size_t idx = BM_IDX(p);
    if (!BM_TEST(mark_bitmap, idx)) {
	BM_SET(mark_bitmap, idx);
	mark_stack_push(p);
    }
}


/*
 * Trace: pop objects from the mark stack, scan their fields,
 * mark any pointer-containing fields.
 */
static void
ms_trace(void)
{
    while (mark_stack_top > 0) {
	ref_t *obj = mark_stack_pop();
	size_t len = ms_get_length(obj);

	/* Sanity: clamp length so we don't read past the heap. */
	if (obj + len > new_space.end)
	    len = (size_t)(new_space.end - obj);

	for (size_t i = 0; i < len; i++) {
	    ref_t val = obj[i];
	    if (val & PTR_MASK)
		ms_mark_ref(val);
	}
    }
}


/*
 * Concurrent variant of ms_mark_ref — uses atomic bitmap operations
 * so the GC thread can mark concurrently with mutator allocations.
 * The mark stack is private to the GC thread so push/pop need not
 * be atomic.
 */
static void
ms_mark_ref_concurrent(ref_t r)
{
    if (!(r & PTR_MASK))
	return;

    ref_t *p = ANY_TO_PTR(r);

    if (TAG_IS(r, LOC_TAG)) {
	if (!NEW_PTR(p))
	    return;
	p = find_object_start(p);
	if (!p) return;
    } else {
	if (!NEW_PTR(p))
	    return;
    }

    size_t idx = BM_IDX(p);
    uint64_t bit = 1ULL << (idx % 64);
    uint64_t old = OAK_ATOMIC_FETCH_OR_U64(&mark_bitmap[idx / 64], bit);
    if (!(old & bit))
	mark_stack_push(p);
}


/*
 * Concurrent trace: pop objects from the mark stack and scan their
 * fields with atomic marking.  The mark stack is only accessed by
 * the GC coordinator thread.
 */
static void
ms_trace_concurrent(void)
{
    while (mark_stack_top > 0) {
	ref_t *obj = mark_stack_pop();
	size_t len = ms_get_length(obj);

	if (obj + len > new_space.end)
	    len = (size_t)(new_space.end - obj);

	for (size_t i = 0; i < len; i++) {
	    ref_t val = obj[i];
	    if (val & PTR_MASK)
		ms_mark_ref_concurrent(val);
	}
    }
}


/*
 * Drain all SATB buffers into the mark stack.
 * Called during the remark STW pause so that objects whose pointers
 * were overwritten during concurrent mark are traced.
 */
void
satb_drain_to_mark_stack(void)
{
#ifdef THREADS
    extern int next_index;
    for (int i = 0; i < next_index; i++) {
	for (size_t j = 0; j < satb_count_array[i]; j++)
	    ms_mark_ref(satb_buffer_array[i][j]);
	satb_count_array[i] = 0;
    }
#else
    for (size_t j = 0; j < satb_count; j++)
	ms_mark_ref(satb_buffer[j]);
    satb_count = 0;
#endif
}


/*
 * Mark roots: VM registers, stacks, spatic space.
 */
static void
ms_mark_roots(void)
{
    ref_t *p;

#ifdef THREADS
    int my_index;
    int gc_thread_count = next_index;
#define FORTHREADS for (my_index=0; my_index<gc_thread_count; my_index++)
#else
#define FORTHREADS
#endif

    /* Global VM registers */
    ms_mark_ref(e_nil);
    ms_mark_ref(e_t);
    ms_mark_ref(e_boot_code);
    ms_mark_ref(e_fixnum_type);
    ms_mark_ref(e_loc_type);
    ms_mark_ref(e_cons_type);
    ms_mark_ref(e_env_type);
    ms_mark_ref(e_object_type);
    ms_mark_ref(e_segment_type);
    ms_mark_ref(e_uninitialized);
    ms_mark_ref(e_method_type);
    ms_mark_ref(e_operation_type);

    /* Tables stored as pointers with offset */
    if (e_subtype_table)
	ms_mark_ref(PTR_TO_REF(e_subtype_table - 2));
    if (e_argless_tag_trap_table)
	ms_mark_ref(PTR_TO_REF(e_argless_tag_trap_table - 2));
    if (e_arged_tag_trap_table)
	ms_mark_ref(PTR_TO_REF(e_arged_tag_trap_table - 2));

    /* Per-thread registers */
    FORTHREADS {
	ms_mark_ref(PTR_TO_REF(e_env));
	ms_mark_ref(e_code_segment);
	ms_mark_ref(e_current_method);
	ms_mark_ref(e_process);
	ms_mark_ref(PTR_TO_LOC(e_bp));

	/* GC examine buffer */
	for (p = gc_examine_buffer; p < gc_examine_ptr; p++)
	    ms_mark_ref(*p);

	/* Value stack */
	for (p = value_stack.bp; p <= value_stack.sp; p++)
	    ms_mark_ref(*p);

	/* Context stack */
	for (p = context_stack.bp; p <= context_stack.sp; p++)
	    ms_mark_ref(*p);

	/* Stack segments (linked lists in the heap) */
	ms_mark_ref(value_stack.segment);
	ms_mark_ref(context_stack.segment);
    }

    /* Scan spatic space: any pointer from spatic into new_space is a root. */
    for (p = spatic.start; p < spatic.end; p++)
	ms_mark_ref(*p);

#undef FORTHREADS
}


/* ------------------------------------------------------------------ */
/*  Sweep phase                                                        */
/* ------------------------------------------------------------------ */

#ifndef THREADS
static size_t
ms_sweep(void)
{
    size_t reclaimed = 0;

    /* Clear all free lists. */
    for (int i = 0; i < NUM_SIZE_CLASSES; i++)
	free_list_heads[i] = NULL;
    free_list_total_words = 0;

    /* Walk the heap using the objstart bitmap.  Only sweep up to
       sweep_limit — objects allocated during concurrent mark (above
       sweep_limit) are implicitly live and must not be swept. */
    ref_t *limit = sweep_limit ? sweep_limit : free_point;
    size_t heap_words = (size_t)(limit - new_space.start);
    size_t bm_words = (heap_words + 63) / 64;

    for (size_t w = 0; w < bm_words; w++) {
	uint64_t os_word = objstart_bitmap[w];
	if (os_word == 0) continue;

	while (os_word != 0) {
	    int bit = CTZ64(os_word);
	    os_word &= os_word - 1;   /* clear lowest set bit */

	    size_t idx = w * 64 + bit;
	    if (idx >= heap_words) break;

	    ref_t *obj = new_space.start + idx;

	    /* Determine object extent by scanning to the next objstart
	       bit.  We cannot use ms_get_length() here because naked
	       cells (from MAKE-CELL) store arbitrary values in word[0]
	       which may coincidentally look like type pointers, giving
	       incorrect lengths.  The objstart bitmap is the
	       authoritative source of allocation boundaries. */
	    size_t next_idx = idx + 1;
	    while (next_idx < heap_words &&
		   !BM_TEST(objstart_bitmap, next_idx))
		next_idx++;
	    size_t len = next_idx - idx;

	    if (BM_TEST(mark_bitmap, idx)) {
		/* Live: clear mark for next cycle. */
		BM_CLR(mark_bitmap, idx);
	    } else {
		/* Dead: add to free list.  1-word objects go to
		   class 0 (no room for a size field); all others
		   store their actual size in word[1] so the
		   allocator can verify the entry is big enough. */
		if (len < 1) len = 1;
		int sc = ms_size_class(len);

		obj[0] = (ref_t)free_list_heads[sc];
		if (len >= 2)
		    obj[1] = (ref_t)len;
		free_list_heads[sc] = obj;
		free_list_total_words += len;
		reclaimed += len;

		/* Note: we leave the objstart bit set so that the
		   free list entry is still findable by
		   find_object_start.  It will be re-used if this
		   entry is allocated.  If the object stays dead,
		   the bit remains set for the next sweep. */
	    }
	}
    }

    return reclaimed;
}
#endif /* !THREADS */


/* ------------------------------------------------------------------ */
/*  Lazy sweep (Phase 5)                                               */
/* ------------------------------------------------------------------ */

static size_t lazy_sweep_cursor;   /* word index into heap */
static bool   lazy_sweep_active;   /* true when blocks remain to sweep */

void
ms_prepare_lazy_sweep(void)
{
    /* Clear all free lists — they will be rebuilt incrementally
       as blocks are lazily swept. */
    for (int i = 0; i < NUM_SIZE_CLASSES; i++)
	free_list_heads[i] = NULL;
    free_list_total_words = 0;

    lazy_sweep_cursor = 0;
    lazy_sweep_active = true;
}


bool
ms_lazy_sweep_one(void)
{
    if (!lazy_sweep_active)
	return false;

    ref_t *limit = sweep_limit ? sweep_limit : free_point;
    size_t heap_words = (size_t)(limit - new_space.start);

    if (lazy_sweep_cursor >= heap_words) {
	lazy_sweep_active = false;
	return false;
    }

    size_t start = lazy_sweep_cursor;
    size_t end = start + SWEEP_BLOCK_SIZE;
    if (end > heap_words)
	end = heap_words;
    lazy_sweep_cursor = end;

    /* Walk objstart bits in [start, end). */
    size_t start_w = start / 64;
    size_t end_w = (end + 63) / 64;

    for (size_t w = start_w; w < end_w; w++) {
	uint64_t os_word = objstart_bitmap[w];
	if (os_word == 0) continue;

	while (os_word != 0) {
	    int bit = CTZ64(os_word);
	    os_word &= os_word - 1;

	    size_t idx = w * 64 + bit;
	    if (idx < start || idx >= end) continue;

	    ref_t *obj = new_space.start + idx;

	    /* Determine object extent by finding next objstart.
	       This may cross block boundaries — that's fine. */
	    size_t next_idx = idx + 1;
	    while (next_idx < heap_words &&
		   !BM_TEST(objstart_bitmap, next_idx))
		next_idx++;
	    size_t len = next_idx - idx;

	    if (BM_TEST(mark_bitmap, idx)) {
		/* Live: clear mark for next cycle. */
		BM_CLR(mark_bitmap, idx);
	    } else {
		/* Dead: add to free list. */
		if (len < 1) len = 1;
		int sc = ms_size_class(len);

		obj[0] = (ref_t)free_list_heads[sc];
		if (len >= 2)
		    obj[1] = (ref_t)len;
		free_list_heads[sc] = obj;
		free_list_total_words += len;
	    }
	}
    }

    return true;
}


void
ms_lazy_sweep_finish(void)
{
    while (ms_lazy_sweep_one())
	;
}


/* ------------------------------------------------------------------ */
/*  Weak pointer handling                                              */
/* ------------------------------------------------------------------ */

static unsigned long
ms_post_gc_wp(void)
{
    unsigned long count = 0;
    long i;

    for (i = 0; i < wp_index; i++) {
	ref_t r = wp_table[1 + i];
	if (r & PTR_MASK) {
	    ref_t *p = ANY_TO_PTR(r);
	    if (NEW_PTR(p)) {
		size_t idx = BM_IDX(p);
		/* For locatives, find the object start. */
		if (TAG_IS(r, LOC_TAG)) {
		    ref_t *obj = find_object_start(p);
		    if (!obj || !BM_TEST(mark_bitmap, BM_IDX(obj))) {
			wp_table[1 + i] = e_nil;
			count++;
		    }
		} else {
		    if (!BM_TEST(mark_bitmap, idx)) {
			wp_table[1 + i] = e_nil;
			count++;
		    }
		}
	    }
	}
    }

    rebuild_wp_hashtable();
    return count;
}


/* ------------------------------------------------------------------ */
/*  Main collection entry point                                        */
/* ------------------------------------------------------------------ */

void
ms_collect(bool pre_dump, char *reason, size_t amount)
{
    long old_used;

#ifdef THREADS
    /* If a concurrent GC is already running (another thread triggered
       collection), wait for it to complete and return.  The caller's
       ALLOCATE_PROT will retry the allocation after we return. */
    if (gc_concurrent_in_progress) {
	oak_mutex_unlock(&alloc_lock);
	ms_wait_concurrent_gc();
	oak_mutex_lock(&alloc_lock);
	return;
    }

    /* We are the GC coordinator for this cycle. */
    gc_concurrent_in_progress = 1;
#endif

    /* Finish any lazy sweep remaining from the previous cycle.
       This must happen before we clear the mark bitmap. */
    ms_lazy_sweep_finish();

    /* For world dumps, use the copying GC which produces a contiguous
       heap suitable for serialization. */
    if (pre_dump) {
	gc(pre_dump, true, reason, amount);
#ifdef THREADS
	oak_mutex_lock(&gc_concurrent_lock);
	gc_concurrent_in_progress = 0;
	oak_cond_broadcast(&gc_concurrent_done_cv);
	oak_mutex_unlock(&gc_concurrent_lock);
#endif
	return;
    }

    old_used = (long)(free_point - new_space.start) -
	(long)free_list_total_words;

    if (trace_gc == 1)
	fprintf(stderr, "\n;MS-GC");
    if (trace_gc > 1)
	fprintf(stderr, "\n; Mark-sweep GC due to %s.\n", reason);

    /* --- Retire TLABs --- */
#ifdef THREADS
    ms_tlab_retire_all();
#endif

    /* Save the current free_point as the sweep limit.  Objects
       allocated above this point during concurrent mark are
       implicitly live and will not be swept. */
    sweep_limit = free_point;

    /* --- Mark phase --- */
    if (trace_gc > 1) {
	fprintf(stderr, "; Marking... [heap=%zu, used=%ld, free_list=%zu]",
		new_space.size, (long)(free_point - new_space.start),
		free_list_total_words);
	fflush(stderr);
    }

    memset(mark_bitmap, 0, bitmap_words * sizeof(uint64_t));
    mark_stack_init();

    /* Disable crash recovery during GC so that any SIGSEGV from bad
       pointers results in an immediate abort rather than longjmping
       back to the interpreter loop (which would re-trigger GC). */
    int saved_crash_recovery = crash_recovery_installed;
    crash_recovery_installed = 0;

#ifdef THREADS
    {
	int my_index_val = *(int *)oak_tls_get(index_key);

	/* === STW PAUSE 1: Initial mark (scan roots) === */
	gc_phase = GC_PHASE_INITIAL_MARK;

	/* Release alloc_lock so blocked threads can see gc_pending
	   in their spin loops and park via wait_for_gc(). */
	oak_mutex_unlock(&alloc_lock);

	(void)stw_request_pause(my_index_val);

	if (trace_gc > 1)
	    fprintf(stderr, "\n; STW initial mark...");

	ms_mark_roots();
	if (trace_gc > 1) { fprintf(stderr, " roots"); fflush(stderr); }

	/* Activate SATB barriers before releasing mutators.
	   This ensures every heap write during concurrent mark
	   logs the old value for the remark phase. */
	gc_satb_active = 1;
	gc_phase = GC_PHASE_CONCURRENT_MARK;

	/* === RELEASE: mutators resume during concurrent mark === */
	stw_release(my_index_val);

	if (trace_gc > 1) {
	    fprintf(stderr, "\n; Concurrent tracing...");
	    fflush(stderr);
	}

	/* === CONCURRENT MARK === */
	ms_trace_concurrent();

	if (trace_gc > 1) {
	    fprintf(stderr, " done.\n; STW remark...");
	    fflush(stderr);
	}

	/* === STW PAUSE 2: Remark === */
	gc_phase = GC_PHASE_REMARK;
	(void)stw_request_pause(my_index_val);

	/* Retire TLABs allocated during concurrent mark. */
	ms_tlab_retire_all();

	/* Drain SATB buffers — these contain old pointer values
	   that were overwritten during concurrent mark and must
	   be traced to maintain the snapshot-at-beginning invariant. */
	satb_drain_to_mark_stack();

	/* Re-scan roots to catch new stack entries that appeared
	   during concurrent mark.  Stacks are C arrays, not covered
	   by SATB barriers. */
	ms_mark_roots();

	/* Scan objects allocated during concurrent mark.  These
	   are above sweep_limit and won't be swept, but they may
	   contain pointers to old objects that need marking. */
	{
	    ref_t *p;
	    for (p = sweep_limit; p < free_point; p++) {
		ref_t val = *p;
		if (val & PTR_MASK)
		    ms_mark_ref(val);
	    }
	}

	/* Process all newly discovered references. */
	ms_trace();

	if (trace_gc > 1) { fprintf(stderr, " done."); fflush(stderr); }

	/* Deactivate SATB barriers. */
	gc_satb_active = 0;
	satb_flush_all();
    }
#else
    /* Single-threaded: STW mark as before. */
    ms_mark_roots();
    if (trace_gc > 1) { fprintf(stderr, " roots"); fflush(stderr); }
    ms_trace();
#endif

    crash_recovery_installed = saved_crash_recovery;

    if (trace_gc > 1)
	fprintf(stderr, "\n; Mark complete.\n");

    /* --- Weak pointers --- */
    if (trace_gc > 1)
	fprintf(stderr, "; Scanning weak pointer table...");
    {
	unsigned long wp_count = ms_post_gc_wp();
	if (trace_gc > 1)
	    fprintf(stderr, " %lu entr%s discarded.\n",
		    wp_count, wp_count != 1 ? "ies" : "y");
    }

    /* --- Sweep phase --- */
    gc_phase = GC_PHASE_SWEEP;

    /* Free the mark stack before sweeping — it's no longer needed. */
    free(mark_stack);
    mark_stack = NULL;
    mark_stack_size = 0;

#ifdef THREADS
    {
	int my_index_val = *(int *)oak_tls_get(index_key);

	/* Prepare lazy sweep: clear free lists and set the sweep
	   cursor to the start of the heap.  Blocks will be swept
	   on demand by allocating threads (ms_lazy_sweep_one). */
	ms_prepare_lazy_sweep();

	if (trace_gc > 1)
	    fprintf(stderr, "; Lazy sweep prepared (%zu words to sweep).\n",
		    (size_t)((sweep_limit ? sweep_limit : free_point)
			     - new_space.start));

	/* Sweep enough blocks now to satisfy the requested amount.
	   This avoids the caller falling through to the copying GC
	   fallback when free lists are still empty. */
	{
	    size_t swept_so_far = 0;
	    while (free_list_total_words < amount &&
		   ms_lazy_sweep_one()) {
		/* keep sweeping */
	    }
	    swept_so_far = free_list_total_words;

	    if (trace_gc > 1 && swept_so_far > 0)
		fprintf(stderr, "; Eagerly swept %zu words.\n",
			swept_so_far);
	}

	gc_phase = GC_PHASE_IDLE;

	/* Release all threads from the remark STW pause. */
	stw_release(my_index_val);

	/* Re-acquire alloc_lock — ALLOCATE_PROT expects us to hold it. */
	oak_mutex_lock(&alloc_lock);

	/* Check if we need the copying GC fallback. */
	{
	    size_t free_space = free_list_total_words +
		(size_t)(new_space.end - free_point);

	    if (trace_gc == 1)
		fprintf(stderr, ":%ld%%",
			old_used > 0
			? (100 * (long)free_list_total_words) / old_used
			: 0);
	    if (trace_gc > 1)
		fprintf(stderr, "; MS-GC complete.  %zu free (%zu list + %zu bump).\n",
			free_space, free_list_total_words,
			(size_t)(new_space.end - free_point));

	    if (free_space < amount) {
		if (trace_gc > 1)
		    fprintf(stderr,
			    "; MS-GC: insufficient space after collection, "
			    "falling back to copying GC.\n");

		/* Finish any remaining lazy sweep before copying GC. */
		ms_lazy_sweep_finish();
		sweep_limit = NULL;

		{
		    long new_used = old_used - (long)free_list_total_words;
		    if (new_used < 0) new_used = 0;
		    size_t live = (size_t)new_used + amount;
		    size_t want = 3 * live;
		    if (want > original_newspace_size) {
			original_newspace_size = want;
			e_next_newspace_size = want;
		    }
		}

		gc(false, true, "mark-sweep fallback", amount);
		ms_reinit();
	    }
	}

	sweep_limit = NULL;

	if (trace_gc == 1)
	    fprintf(stderr, "\n");

	/* Signal any threads waiting for the concurrent GC to finish. */
	oak_mutex_lock(&gc_concurrent_lock);
	gc_concurrent_in_progress = 0;
	oak_cond_broadcast(&gc_concurrent_done_cv);
	oak_mutex_unlock(&gc_concurrent_lock);
    }
#else
    /* Single-threaded: full STW sweep. */
    if (trace_gc > 1)
	fprintf(stderr, "; Sweeping...");

    size_t reclaimed = ms_sweep();

    if (trace_gc > 1)
	fprintf(stderr, " %zu words reclaimed.\n", reclaimed);

    gc_phase = GC_PHASE_IDLE;
    sweep_limit = NULL;

    /* --- Post-GC sizing --- */
    {
	long new_used = old_used - (long)reclaimed;
	if (new_used < 0) new_used = 0;
	size_t free_space = free_list_total_words +
	    (size_t)(new_space.end - free_point);

	if (trace_gc == 1)
	    fprintf(stderr, ":%ld%%",
		    old_used > 0 ? (100 * (long)reclaimed) / old_used : 0);
	if (trace_gc > 1)
	    fprintf(stderr, "; MS-GC complete.  %ld used, %zu free (%zu list + %zu bump).\n",
		    (long)new_used, free_space,
		    free_list_total_words,
		    (size_t)(new_space.end - free_point));

	/* If free space is still insufficient, expand the heap. */
	if (free_space < amount) {
	    if (trace_gc > 1)
		fprintf(stderr,
			"; MS-GC: insufficient space after collection, "
			"falling back to copying GC.\n");

	    {
		size_t live = (size_t)new_used + amount;
		size_t want = 3 * live;
		if (want > original_newspace_size) {
		    original_newspace_size = want;
		    e_next_newspace_size = want;
		}
	    }

	    gc(false, true, "mark-sweep fallback", amount);
	    ms_reinit();
	}

	if (trace_gc == 1)
	    fprintf(stderr, "\n");
    }
#endif
}


/* ------------------------------------------------------------------ */
/*  Initialization                                                     */
/* ------------------------------------------------------------------ */

void
ms_init(void)
{
    size_t heap_words = new_space.size;
    bitmap_words = (heap_words + 63) / 64;

    objstart_bitmap = (uint64_t *)xmalloc(bitmap_words * sizeof(uint64_t));
    mark_bitmap = (uint64_t *)xmalloc(bitmap_words * sizeof(uint64_t));

    memset(objstart_bitmap, 0, bitmap_words * sizeof(uint64_t));
    memset(mark_bitmap, 0, bitmap_words * sizeof(uint64_t));

    for (int i = 0; i < NUM_SIZE_CLASSES; i++)
	free_list_heads[i] = NULL;
    free_list_total_words = 0;
}

void
ms_destroy(void)
{
    if (objstart_bitmap) { free(objstart_bitmap); objstart_bitmap = NULL; }
    if (mark_bitmap) { free(mark_bitmap); mark_bitmap = NULL; }
    bitmap_words = 0;
}

void
ms_reinit(void)
{
    ms_destroy();
    ms_init();

    /* Apply objstart bits recorded during the copying GC's transport
       phase.  Without this, objects copied by gc() would have no
       objstart bits and be invisible to ms_sweep. */
    for (size_t i = 0; i < deferred_count; i++) {
	ref_t *p = deferred_objstarts[i];
	if (p >= new_space.start && p < new_space.end)
	    BM_SET(objstart_bitmap, BM_IDX(p));
    }
    if (deferred_objstarts) {
	free(deferred_objstarts);
	deferred_objstarts = NULL;
    }
    deferred_count = 0;
    deferred_cap = 0;
}


bool
ms_is_heap_ptr(const ref_t *p)
{
    return p >= new_space.start && p < new_space.end;
}
