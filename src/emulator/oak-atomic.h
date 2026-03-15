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
 * oak-atomic.h - Portable atomic operations, bit intrinsics, and
 *                branch hints.
 *
 * Strategy: prefer C11 <stdatomic.h>, fall back to GCC __atomic_*
 * builtins, then to MSVC _Interlocked* intrinsics.
 */

#ifndef OAK_ATOMIC_H_INCLUDED
#define OAK_ATOMIC_H_INCLUDED

#include <stdint.h>
#include <stddef.h>


/* ------------------------------------------------------------------ */
/*  Branch prediction hints                                            */
/* ------------------------------------------------------------------ */

#if defined(__GNUC__) || defined(__clang__)
#define OAK_UNLIKELY(x)  __builtin_expect((x), 0)
#define OAK_LIKELY(x)    __builtin_expect((x), 1)
#else
#define OAK_UNLIKELY(x)  (x)
#define OAK_LIKELY(x)    (x)
#endif


/* ------------------------------------------------------------------ */
/*  Count leading/trailing zeros (64-bit)                              */
/* ------------------------------------------------------------------ */

#if defined(__GNUC__) || defined(__clang__)
#define CLZ64(x)  __builtin_clzll(x)
#define CTZ64(x)  __builtin_ctzll(x)
#elif defined(_MSC_VER)
#include <intrin.h>
static inline int CLZ64(uint64_t x) {
    unsigned long idx;
    _BitScanReverse64(&idx, x);
    return 63 - (int)idx;
}
static inline int CTZ64(uint64_t x) {
    unsigned long idx;
    _BitScanForward64(&idx, x);
    return (int)idx;
}
#else
/* Generic fallback */
static inline int CLZ64(uint64_t x) {
    int n = 0;
    if (!(x & 0xFFFFFFFF00000000ULL)) { n += 32; x <<= 32; }
    if (!(x & 0xFFFF000000000000ULL)) { n += 16; x <<= 16; }
    if (!(x & 0xFF00000000000000ULL)) { n +=  8; x <<=  8; }
    if (!(x & 0xF000000000000000ULL)) { n +=  4; x <<=  4; }
    if (!(x & 0xC000000000000000ULL)) { n +=  2; x <<=  2; }
    if (!(x & 0x8000000000000000ULL)) { n +=  1; }
    return n;
}
static inline int CTZ64(uint64_t x) {
    int n = 0;
    if (!(x & 0x00000000FFFFFFFFULL)) { n += 32; x >>= 32; }
    if (!(x & 0x000000000000FFFFULL)) { n += 16; x >>= 16; }
    if (!(x & 0x00000000000000FFULL)) { n +=  8; x >>=  8; }
    if (!(x & 0x000000000000000FULL)) { n +=  4; x >>=  4; }
    if (!(x & 0x0000000000000003ULL)) { n +=  2; x >>=  2; }
    if (!(x & 0x0000000000000001ULL)) { n +=  1; }
    return n;
}
#endif


/* ------------------------------------------------------------------ */
/*  Atomic operations                                                  */
/* ------------------------------------------------------------------ */

/*
 * Three tiers:
 *   1. C11 <stdatomic.h> (GCC 4.9+, Clang 3.1+, MSVC 17.5+)
 *   2. GCC/Clang __atomic_* builtins
 *   3. MSVC _Interlocked* intrinsics
 *
 * Operations provided:
 *   OAK_ATOMIC_FETCH_OR_U64(ptr, val)   - atomic OR on uint64_t, relaxed
 *   OAK_ATOMIC_LOAD_REF(ptr)            - atomic load with acquire semantics
 *   OAK_ATOMIC_STORE_REF(ptr, val)      - atomic store with release semantics
 */

#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L \
     && !defined(__STDC_NO_ATOMICS__)) \
    || defined(HAVE_STDATOMIC_H)

#include <stdatomic.h>

#define OAK_ATOMIC_FETCH_OR_U64(ptr, val) \
    atomic_fetch_or_explicit((_Atomic uint64_t *)(ptr), (val), \
			     memory_order_relaxed)

#define OAK_ATOMIC_LOAD_REF(ptr) \
    atomic_load_explicit((_Atomic size_t *)(ptr), memory_order_acquire)

#define OAK_ATOMIC_STORE_REF(ptr, val) \
    atomic_store_explicit((_Atomic size_t *)(ptr), (val), \
			  memory_order_release)

#elif defined(__GNUC__) || defined(__clang__)

#define OAK_ATOMIC_FETCH_OR_U64(ptr, val) \
    __atomic_fetch_or((ptr), (val), __ATOMIC_RELAXED)

#define OAK_ATOMIC_LOAD_REF(ptr) \
    __atomic_load_n((ptr), __ATOMIC_ACQUIRE)

#define OAK_ATOMIC_STORE_REF(ptr, val) \
    __atomic_store_n((ptr), (val), __ATOMIC_RELEASE)

#elif defined(_MSC_VER)

#include <intrin.h>

#define OAK_ATOMIC_FETCH_OR_U64(ptr, val) \
    _InterlockedOr64((volatile long long *)(ptr), (long long)(val))

static inline size_t oak_atomic_load_ref_(volatile size_t *p) {
    size_t v = *p;
    _ReadBarrier();
    return v;
}
#define OAK_ATOMIC_LOAD_REF(ptr) \
    oak_atomic_load_ref_((volatile size_t *)(ptr))

static inline void oak_atomic_store_ref_(volatile size_t *p, size_t v) {
    _WriteBarrier();
    *p = v;
}
#define OAK_ATOMIC_STORE_REF(ptr, val) \
    oak_atomic_store_ref_((volatile size_t *)(ptr), (val))

#else
#error "No atomic operations available.  Need C11, GCC, Clang, or MSVC."
#endif


#endif /* OAK_ATOMIC_H_INCLUDED */
