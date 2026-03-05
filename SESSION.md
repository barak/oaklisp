# Oaklisp 64-bit Port — Session State

## What Was Done

Oaklisp has been ported from 32-bit-only to native 64-bit using "mode (b)":
64-bit refs with 2 instructions per ref (instructions in low 32 bits, upper
32 bits empty). 64-bit is now the default build mode. Version bumped to 2.0.0.

### Changes made (all uncommitted)

12 modified files, 3 untracked:

**Emulator (C):**
- `src/emulator/data.h` — REF_SHIFT, INSTRS_PER_REF, INSTR_STRIDE constants; widened REF_TO_INT, INT_TO_REF, OVERFLOWN_INT, wp_to_ref for 64-bit
- `src/emulator/loop.c` — INCREMENT_PC gap-skipping; __int128 TIMES overflow; LOAD-IMM alignment; <0? and LOAD-IMM-FIX widening
- `src/emulator/stacks-loop.h` — 64-bit PUSH_CONTEXT using INCREMENT_PC for physical offsets
- `src/emulator/gc.c` — pc_touch alignment mask for 8-byte refs
- `src/emulator/worldio.c` — REF_SHIFT in contig(); 64-bit magic bytes; format strings; world validation
- `src/emulator/weak.c` — 64-bit Fibonacci hash constant

**Build system:**
- `configure.ac` — 64-bit default, version 2.0.0, AM_CONDITIONAL BITS64, __int128 check
- `src/world/Makefile.am` — COLD_TARGET_FLAG for 64-bit cold world building

**Oaklisp runtime:**
- `src/world/tool.oak` — Auto-detects 64-bit from most-positive-fixnum; tagize-ptr/tagize-loc use REF_SHIFT
- `src/world/bignum.oak` — Runtime detection of most-negative-fixnum (29-bit vs 61-bit)
- `src/world/print-integer.oak` — Delegates 10+ digit fixnums to generic integer method

**New/untracked files:**
- `32-to-64-bit.md` — Comprehensive technical documentation of the port
- `BUILD.md` — Rewritten build instructions (was already tracked, heavily modified)
- `CLAUDE.md` — Project guide for AI assistants
- `prebuilt/src/world/el64/oakworld.bin` — Prebuilt 64-bit world image
- `build32/oaklisp` — 32-bit emulator (build artifact)
- `build64/oaklisp` — 64-bit emulator (build artifact)

### Build artifacts present

- `build64/oaklisp` — Working 64-bit emulator
- `build32/oaklisp` — Working 32-bit emulator
- `prebuilt/src/world/el64/oakworld.bin` — 64-bit prebuilt world (1043476 bytes)
- `src/world/oakworld{-1,-2,-3,}.bin` — All 4 world build stages (64-bit)
- `src/world/*.oa` — Compiled bytecode files (portable across architectures)

## What Works

- 64-bit emulator builds and runs
- Cold world generation (self-hosted, auto-detects 64-bit)
- All 4 world build stages complete
- Arithmetic: fixnums (62-bit), bignums, rationals
- Overflow detection: fixnum multiply via __int128
- GC works (tested with small heaps)
- World dump/reload cycle works
- Compilation works under 64-bit
- 32-bit mode still works (tested)

## Known Issues

1. **64-bit compiler CDR bug**: **FIXED.** Root cause was undefined behavior
   in `CHECKVAL_POP` and `CHECKCXT_POP` macros in `stacks-loop.h`: the pointer
   comparison `&local_value_sp[-(n)] < value_stack_bp` is UB when the
   subtraction goes below the array base; GCC -O2 on 64-bit optimized it away,
   so `VALUE_UNFLUSH` never triggered.  Fixed by replacing with integer
   arithmetic: `(n) > (long)(local_value_sp - value_stack_bp)`.

2. **No autotools regeneration done**: `configure.ac` and `Makefile.am` have
   been modified but `autoreconf` has not been run. The manual builds in
   build32/ and build64/ bypass autotools entirely.

3. **No eb64 prebuilt world**: Only little-endian 64-bit world exists.
   Big-endian 64-bit would need cross-building.

4. **Nothing committed**: All changes are in the working tree.

## How to Resume

### Verify everything still works:

    cd /drive2/ROOT/home/blake/Backup/oaklisp.git
    build64/oaklisp --world prebuilt/src/world/el64/oakworld.bin -- \
      --eval '(format #t "~A~%" (expt 2 100))' --exit

### Rebuild 64-bit emulator (if needed):

    gcc -DHAVE_CONFIG_H -DFAST -I. -O2 -Wall \
      -o build64/oaklisp \
      src/emulator/oaklisp.c src/emulator/cmdline.c src/emulator/gc.c \
      src/emulator/loop.c src/emulator/stacks.c src/emulator/timers.c \
      src/emulator/weak.c src/emulator/worldio.c src/emulator/xmalloc.c \
      src/emulator/instr.c

    Requires config.h in project root with:
      #define PACKAGE_STRING "oaklisp 2.0.0"
      #define HAVE_CONFIG_H 1
      #define HAVE___INT128 1
      #define SIZEOF_VOID_P 8

### Rebuild 32-bit emulator (if needed):

    gcc -m32 -DHAVE_CONFIG_H -DFAST -I. -O2 -Wall \
      -o build32/oaklisp \
      src/emulator/oaklisp.c src/emulator/cmdline.c src/emulator/gc.c \
      src/emulator/loop.c src/emulator/stacks.c src/emulator/timers.c \
      src/emulator/weak.c src/emulator/worldio.c src/emulator/xmalloc.c \
      src/emulator/instr.c

### Compile .oa files (use 32-bit world — it has the compiler and avoids the CDR bug):

    cd src/world
    ../../build32/oaklisp --world ../../prebuilt/src/world/el32/oakworld.bin -- \
      --compile FILENAME --exit

### Build complete 64-bit world from scratch:

    cd src/world

    # Stage 0: cold world (tool.oak auto-detects 64-bit)
    ../../build64/oaklisp --world ../../prebuilt/src/world/el64/oakworld.bin -- \
      --load tool \
      --eval "(tool-files '($(cat Makefile-vars | grep '^COLDFILESD' | \
        sed 's/COLDFILESD = //; s/\.oa//g')) 'new)" --exit

    # Stage 1: boot cold world
    ../../build64/oaklisp --world new.cold --dump oakworld-1.bin -- --exit

    # Stage 2: load MISCFILES
    ../../build64/oaklisp --world oakworld-1.bin --dump oakworld-2.bin -- \
      $(grep '^MISCFILES' Makefile-vars | sed 's/MISCFILES = //; s/\([^ ]*\)\.oa/--load \1/g') \
      --exit

    # Stage 3: load COMPFILES into compiler-locale
    ../../build64/oaklisp --world oakworld-2.bin --dump oakworld-3.bin -- \
      --locale compiler-locale \
      $(grep '^COMPFILES' Makefile-vars | sed 's/COMPFILES = //; s/\([^ ]*\)\.oa/--load \1/g') \
      --locale system-locale --exit

    # Stage 4: load RNRSFILES into scheme-locale
    ../../build64/oaklisp --world oakworld-3.bin --dump oakworld.bin -- \
      --eval '(define-instance scheme-locale locale (list system-locale))' \
      --locale scheme-locale \
      $(grep '^RNRSFILES' Makefile-vars | sed 's/RNRSFILES = //; s/\([^ ]*\)\.oa/--load \1/g') \
      --locale system-locale --exit

## Possible Next Steps

- Investigate and fix the 64-bit compiler CDR bug
- Run autoreconf and test the full autotools build flow
- Commit the changes
- Run the unit tests (src/misc/unit-testing.oak, src/misc/testing-tests.oak)
- Build a big-endian 64-bit world (eb64)
- Update debian packaging for 64-bit
- Investigate bignum base optimization (base 10000 -> base 1000000000 for 64-bit)
