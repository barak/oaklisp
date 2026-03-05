# Oaklisp 64-bit Port: Mode (b) — 2 Instructions per 64-bit Ref

## Overview

This documents the port of Oaklisp from 32-bit to 64-bit using "mode (b)":
widen `ref_t` from 32 to 64 bits while keeping 2 instructions per ref. Each
64-bit ref holds two 16-bit instructions in its low 32 bits; the upper 32
bits are empty. The PC skips these gaps. This avoids changing the assembler
or compiler, gives 64-bit pointers, >4GB RAM, and 62-bit fixnums.

64-bit is now the default build mode. On 64-bit hosts, `./configure` builds
natively without `-m32`. Use `--disable-64-bit` to force 32-bit mode.

---

## Critical Architectural Concepts

### TAGSIZE vs REF_SHIFT

This is the single most important distinction in the port:

- **TAGSIZE** (always 2): Number of tag bits in a ref value. Unchanged across architectures. Used for encoding/decoding fixnums and tags.
- **REF_SHIFT** = log2(sizeof(ref_t)): 2 on 32-bit, 3 on 64-bit. Used for converting word indices to byte offsets.

On 32-bit, TAGSIZE == REF_SHIFT == 2. This is a **coincidence**, not a design invariant. The original code used TAGSIZE in places where REF_SHIFT was needed (notably `worldio.c:contig()` and `tool.oak:tagize-ptr/tagize-loc`). On 64-bit this causes segfaults because pointers are constructed with the wrong byte alignment.

### INSTRS_PER_REF vs INSTR_STRIDE

- **INSTRS_PER_REF** (always 2): Logical instructions per ref cell. Unchanged.
- **INSTR_STRIDE** = sizeof(ref_t) / sizeof(instr_t): 2 on 32-bit, 4 on 64-bit. Physical `instr_t` slots per ref cell.

On 64-bit, each ref has 4 `instr_t` (16-bit) slots but only the first 2 hold instructions. The PC must skip the 2-slot gap when advancing across ref boundaries.

### 62-bit Fixnums

On 64-bit, fixnums have 62-bit signed range: [-2^61, 2^61 - 1]. This means:
- `most-positive-fixnum` = 2305843009213693951
- `most-negative-fixnum` = -2305843009213693952
- `(fact 20)` = 2432902008176640000 fits in a single fixnum (vs bignum on 32-bit)
- Two 62-bit fixnums multiplied can overflow 64 bits, requiring `__int128` for detection

---

## Files Changed

### `src/emulator/data.h` — Core Type Definitions

- Added `#include <stdint.h>` for `uintptr_t`
- Added constants: `REF_SHIFT`, `INSTRS_PER_REF`, `INSTR_STRIDE`
- **REF_TO_INT**: `(int32_t)r >> TAGSIZE` → `(ssize_t)(r) >> TAGSIZE`
- **INT_TO_REF**: `(int32_t)(i) << TAGSIZE` → `(ssize_t)(i) << TAGSIZE`
- **OVERFLOWN_INT**: Replaced hardcoded `0xe0000000` with `__WORDSIZE`-based generic version
- **wp_to_ref**: `(u_int32_t)` casts → `(size_t)`
- `MIN_REF`/`MAX_REF` already used `__WORDSIZE` — no change needed

### `src/emulator/loop.c` — Core Bytecode Interpreter

**INCREMENT_PC** (most complex change): On 64-bit, the simple `(pc) += (i)` doesn't work because of the 2-slot gap in each 8-byte ref. The `_advance_pc()` inline function maps logical instruction distances to physical pointer offsets:

```c
static inline instr_t* _advance_pc(instr_t* pc, int n) {
  unsigned sub = ((uintptr_t)pc / sizeof(instr_t)) % INSTR_STRIDE;
  int total = (int)sub + n;
  int ref_delta = (total >= 0)
    ? total / INSTRS_PER_REF
    : -((INSTRS_PER_REF - 1 - total) / INSTRS_PER_REF);
  int new_sub = total - ref_delta * INSTRS_PER_REF;
  return pc + ref_delta * INSTR_STRIDE + new_sub - (int)sub;
}
```

**TIMES** (case 5): Uses `__int128` to detect overflow when multiplying two 62-bit values:

```c
__int128 a = (__int128)REF_TO_INT(x) * (__int128)REF_TO_INT(y);
long highcrap = (long)(a >> (__WORDSIZE - (TAGSIZE + 1)));
if ((highcrap != 0L) && (highcrap != -1L))
    TRAP1(2);
PEEKVAL() = INT_TO_REF((ssize_t)a);
```

**LOAD-IMM** (case 6) and **LOAD-IMM-CON** (case 39):
- Alignment check: `& 0x2` → `& (sizeof(ref_t) - 1)`
- Skip inline ref: hardcoded 2 → `INSTRS_PER_REF`

**`<0?`** (case 18): `(int32_t)x < 0` → `(ssize_t)x < 0`

**LOAD-IMM-FIX** (case 10): `(ssize_t)((int16_t) instr) >> 6` for proper 64-bit sign extension

### `src/emulator/stacks-loop.h` — Stack Macros

**PUSH_CONTEXT**: Separate 32-bit/64-bit paths. The 64-bit path uses `INCREMENT_PC` to compute the physical target address from a logical instruction offset:

```c
{ instr_t *_tgt = local_e_pc;
  INCREMENT_PC(_tgt, (off_set));
  local_context_sp[1] = INT_TO_REF((unsigned long)_tgt -
      (unsigned long)e_code_segment);
}
```

The 32-bit path keeps the original `((off_set)<<1)` byte offset calculation.

### `src/emulator/gc.c` — Garbage Collector

**pc_touch()**: Alignment mask changed from `~TAG_MASKL` to `~(uintptr_t)(sizeof(ref_t) - 1)` for 8-byte alignment on 64-bit. The mask must clear 3 bits (not 2) to find the containing ref cell.

### `src/emulator/worldio.c` — World Image I/O

- **contig()**: `<< TAGSIZE` → `<< REF_SHIFT` (the critical segfault fix)
- **contigify/CONTIGIFY**: `0x2` → `PTR_MASK` (use named constant)
- **read_ref()**: `unsigned long` → `unsigned long long`, `%lx` → `%llx`
- **dump_binary_world()**: 64-bit magic bytes `\004\004\004\004` (vs `\002\002\002\002` for 32-bit)
- **dump_ascii_world()**: `%ld/%lx` → `%zd/%zx` with `(size_t)` casts
- **read_world()**: Validates binary magic matches pointer size, rejects mismatched worlds

### `src/emulator/weak.c` — Weak Pointer Hash

64-bit Fibonacci hash constant:

```c
#if __WORDSIZE == 64
#define wp_key(r) ((unsigned long) 0x9E3779B97F4A7C15UL*(r))
#else
#define wp_key(r) ((unsigned long) 0x9E3779BB*(r))
#endif
```

### `configure.ac` — Build System

- Changed `--enable-64-bit` default from `no` to `yes`
- Removed "experimental" warnings for 64-bit mode
- Added `AM_CONDITIONAL([BITS64], ...)` for Makefile conditionals
- Added `AC_CHECK_TYPES([__int128])` for multiplication overflow detection

### `src/world/tool.oak` — Cold World Builder

- **Auto-detects 64-bit** from running emulator: `(not (= most-positive-fixnum 536870911))`
- Can be overridden with `--eval '(set! target-64bit #t)'` or `#f` after loading
- **tagize-int**: Fixnum range adapts to 62 or 30 bits based on `target-64bit`
- **tagize-ptr**: `(ash-left x (if target-64bit 3 2))` — the Oaklisp equivalent of REF_SHIFT
- **tagize-loc**: Same as tagize-ptr
- **print-hex**: `max-val` adapts for 64-bit

**Important**: `target-64bit` checks must be inline at call time, not precomputed at load time. The `(if target-64bit 3 2)` pattern is inline in each function, not a precomputed variable.

### `src/world/Makefile.am` — Build System Integration

- Added `COLD_TARGET_FLAG` conditional: when `BITS64` (from configure), passes `--eval '(set! target-64bit #t)'` to the cold world build rule
- Handles cross-building (32-bit emulator generating 64-bit cold world)
- For self-hosted builds, tool.oak auto-detects from `most-positive-fixnum`

### `src/world/bignum.oak` — Bignum Arithmetic

`most-negative-fixnum` was hardcoded as `(ash-left 1 29)`, which relies on 32-bit wrapping to produce -2^29. On 64-bit, this stays positive (536870912), making `most-positive-fixnum` negative and causing infinite loops in `normalize-digitlist`.

Fix uses runtime detection:

```oaklisp
(define most-negative-fixnum
  (let ((try (ash-left (let ((x 1)) x) 29)))
    (if (negative? try)
        try
        (ash-left (let ((x 1)) x) 61))))
```

The `(let ((x 1)) x)` prevents the compiler from constant-folding the shift at compile time, ensuring it executes at runtime on the target architecture.

### `src/world/print-integer.oak` — Integer Printing

The fixnum print method had a hardcoded chain of digit-place functions for 1-9 digits. The `else` clause incorrectly called `(d9 self)` for numbers >= 10^9, producing garbled output since `digit->char` received multi-digit quotients.

Fix: Added `(< self 1000000000)` guard for `d9`, with fallback `(^super integer print self stream)` to delegate 10+ digit fixnums to the generic integer method (which uses successive division).

---

## Bugs Discovered and Fixed (in order)

### 1. Segfault on Cold World Boot

**Symptom**: 64-bit emulator segfaults immediately when loading cold world.

**Root cause**: `worldio.c:contig()` used `<< TAGSIZE` (shift by 2) to convert word indices to byte offsets. On 64-bit, needs `<< REF_SHIFT` (shift by 3) because each ref is 8 bytes. Same issue in `tool.oak:tagize-ptr/tagize-loc`.

### 2. Infinite Loop Loading bignum.oa

**Symptom**: 64-bit oakworld-1 hangs with exponential heap growth (0% GC utilization) when loading bignum.oa.

**Root cause**: `bignum.oak` hardcoded `most-negative-fixnum` as `(ash-left 1 29)`. On 64-bit, this produces +536870912 instead of -536870912, making `most-positive-fixnum` = -536870913 (negative!). `normalize-digitlist` enters infinite loop because `quotientm(-1, base)` returns -1, carry never reaches 0.

### 3. Garbled Printing of Large Fixnums

**Symptom**: Numbers like `(* 123456789 987654321)` print as garbage characters.

**Root cause**: `print-integer.oak` fixnum optimization handles only up to 9 digits. The `else` clause for >= 10^9 called `(d9 self)` which passed a multi-digit quotient to `digit->char`, producing non-printable characters.

### 4. Multiplication Overflow Not Detected

**Symptom**: `(expt 2 100)` returns 0, `(fact 20)` returns wrong value.

**Root cause**: Initial `__int128` implementation used `int` (32-bit) variable `highcrap` to hold the overflow check bits, truncating them. Second attempt tried multiplying tagged refs directly and checking top 3 bits of 128-bit product, but overflow bits are at position ~65, not ~125.

**Final fix**: Multiply `REF_TO_INT` values via `__int128`, store overflow check in `long` (64-bit):
```c
long highcrap = (long)(a >> (__WORDSIZE - (TAGSIZE + 1)));
```
This right-shifts by 61, leaving the sign bits that indicate overflow.

---

## Bootstrap Process

### Normal build (self-hosting, 64-bit host)

With the prebuilt 64-bit world in `prebuilt/src/world/el64/`, the standard
`autoreconf && ./configure && make` works. tool.oak auto-detects 64-bit from
the running emulator's `most-positive-fixnum`.

### Cross-building from 32-bit

If no prebuilt 64-bit world is available, cross-build from 32-bit:

1. **Build 32-bit emulator**: `gcc -m32 -DHAVE_CONFIG_H -DFAST -I. -O2 -o build32/oaklisp src/emulator/*.c`
2. **Build 64-bit emulator**: `gcc -DHAVE_CONFIG_H -DFAST -I. -O2 -o build64/oaklisp src/emulator/*.c` (no `-m32`)
3. **Compile .oa files** (using 32-bit oaklisp with full prebuilt world — `.oa` files are portable ASCII S-expressions)
4. **Generate 64-bit cold world**: Run tool.oak under 32-bit oaklisp with `(set! target-64bit #t)`:
   ```
   build32/oaklisp --world prebuilt/src/world/el32/oakworld.bin -- \
     --load src/world/tool.oak \
     --eval '(set! target-64bit #t)' \
     --eval '(cold-world)' --exit
   ```
5. **Boot cold world** → oakworld-1.bin:
   ```
   build64/oaklisp --world src/world/new.cold --dump src/world/oakworld-1.bin -- --exit
   ```
6. **Build remaining stages**: Load MISCFILES → oakworld-2.bin, COMPFILES → oakworld-3.bin, RNRSFILES → oakworld.bin
7. **Install prebuilt**: Copy oakworld.bin to `prebuilt/src/world/el64/oakworld.bin`

### Key bootstrap facts

- `.oa` files are portable across architectures (ASCII S-expression format)
- tool.oak auto-detects 64-bit from the running emulator; for cross-building, set `target-64bit` explicitly
- All compilation of .oak → .oa can be done with either 32-bit or 64-bit system
- Only the emulator execution and world building require the 64-bit emulator

---

## Build Instructions

### Standard build (autotools)

```sh
autoreconf --install
./configure          # 64-bit is now the default
make
```

Use `./configure --disable-64-bit` to force 32-bit mode (requires gcc-multilib).

### Manual 64-bit build (without autotools)

Create `config.h` in project root:
```c
#define PACKAGE_STRING "oaklisp 1.3.7"
#define HAVE_CONFIG_H 1
#define HAVE___INT128 1
#define SIZEOF_VOID_P 8
// Do NOT define HAVE_LONG_LONG — __int128 path is preferred
```

Build:
```sh
gcc -DHAVE_CONFIG_H -DFAST -I. -O2 -Wall \
  -o build64/oaklisp \
  src/emulator/oaklisp.c src/emulator/cmdline.c src/emulator/gc.c \
  src/emulator/loop.c src/emulator/stacks.c src/emulator/timers.c \
  src/emulator/weak.c src/emulator/worldio.c src/emulator/xmalloc.c \
  src/emulator/instr.c
```

### Heap size warning

On 64-bit, `--size-heap N` means N × 1024 refs × 8 bytes. The default (128K refs = 1MB) is fine. `--size-heap 4096` would mean 32GB — almost certainly OOM.

---

## Test Results (64-bit)

| Test | Result |
|------|--------|
| `(+ 1 2)` | 3 |
| `(* 123456789 987654321)` | 121932631112635269 (fits in fixnum) |
| `most-positive-fixnum` | 2305843009213693951 (2^61-1) |
| `most-negative-fixnum` | -2305843009213693952 (-2^61) |
| `(+ most-positive-fixnum 1)` | 2305843009213693952 (bignum, overflow detected) |
| `(expt 2 100)` | 1267650600228229401496703205376 (31-digit bignum) |
| `(fact 20)` | 2432902008176640000 (fixnum on 64-bit!) |
| `(fact 100)` | Correct 158-digit bignum |
| `(length "Hello, 64-bit Oaklisp!")` | 22 |
| World dump/reload | Works |
| GC with 64KB heap | Works |
| Compiler self-compile (bignum.oak) | Works |

---

## Things That Did NOT Need Changing

- **String packing** (`xmalloc.c`): 3-chars-per-ref packing uses shifts at positions 2, 10, 18 — works identically for 64-bit refs since characters occupy the same low bit positions.
- **Code vectors** (`code-vector.oak`): Still 2 instructions per ref. `%vref`, `%pointer`, `%tag` work because instructions occupy the same bit positions within each ref's low 32 bits.
- **MIN_REF/MAX_REF** (`data.h`): Already used `__WORDSIZE`.
- **contig/reoffset** (`worldio.c`): Pointer arithmetic adapts naturally to 64-bit `ref_t` (after the REF_SHIFT fix).
- **POP_CONTEXT** (`stacks-loop.h`): Restores PC from stored physical byte offset — works unchanged.

---

## Gotchas for Future Work

1. **Never confuse TAGSIZE and REF_SHIFT.** TAGSIZE is about value encoding; REF_SHIFT is about memory layout. They're equal only on 32-bit.

2. **tool.oak `target-64bit` auto-detects but can be overridden.** For self-hosting, auto-detection works. For cross-building (32-bit emulator producing 64-bit world), set it explicitly after loading. Derived values must use `(if target-64bit ...)` inline, not precomputed variables.

3. **Hardcoded bit widths in Oaklisp source.** `(ash-left 1 29)` for most-negative-fixnum, digit chains in print-integer — any place that assumes 30-bit fixnums or 32-bit refs needs attention.

4. **`__int128` is essential.** Without it, 62-bit × 62-bit multiplication overflow cannot be detected. The `HAVE_LONG_LONG` fallback (`int64_t`) is insufficient for 64-bit fixnums.

5. **Heap size units.** `--size-heap` is in units of 1024 refs, not bytes. On 64-bit, each ref is 8 bytes, so the memory consumed is 8× what it would be on 32-bit for the same numeric argument.

6. **`(ash-left 1 N)` wraps silently on 32-bit.** `(ash-left 1 32)` returns 1 (not 4294967296) on 32-bit Oaklisp. Don't use large shifts for architecture detection; compare `most-positive-fixnum` against a known 30-bit fixnum value instead.

7. **Compiler CDR error on 64-bit.** The 64-bit world's compiler currently can't compile `tool.oak` (CDR dispatch error). Use the 32-bit world to compile `.oa` files — they're portable.
