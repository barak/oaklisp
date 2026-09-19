# CLAUDE.md - Oaklisp Project Guide

## Project Overview

Oaklisp is a self-hosting, object-oriented dialect of Scheme (Lisp-1) featuring first-class types, multiple inheritance, multiple namespaces (locales), and Common Lisp-style macros. It compiles Oaklisp source to bytecode executed by a C virtual machine. Created by Barak A. Pearlmutter and Kevin J. Lang (since 1986). Licensed GPL-2.0+.

## Repository Structure

```
src/emulator/       C bytecode VM/emulator (~6K lines)
src/world/          Oaklisp runtime + compiler (~145 .oak files, ~22K lines)
src/misc/           Contributed code (unit testing, examples)
doc/lang/           Language manual (LaTeX)
doc/lim/            Implementation manual (LaTeX)
doc/summary/        Concise programmer reference (LaTeX)
doc/examples/       Example Oaklisp programs
man/man1/           Man page template (oaklisp.1.in)
prebuilt/           Prebuilt bootstrap artifacts, on the "master" branch only
m4/                 Cached AX_* autoconf macros, on the "master" branch only
debian/             Debian packaging
```

### Branches

- `devel` — development: source only, never `prebuilt/` or `m4/`
- `master` — `devel` plus the generated material: `prebuilt/` (bytecode,
  world images, PDFs, instr-data.c) and the cached `m4/` macros
- `pristine-tar` — Debian pristine-tar data

The rule: commit source changes on `devel`; every so often merge `devel`
into `master` and follow the merge with a commit that refreshes
`prebuilt/` (`make prebuilt`).  Commit messages on `devel` don't discuss
prebuilt content; that goes in the refresh commit on `master`.

`devel` and release tarballs (`make dist` deliberately omits `prebuilt/`)
build from source alone with Guile 3 (`--with-compile=guile`, chosen
automatically when no world or prebuilt bytecode is found); that is how
the Debian package is built.  `master` builds with nothing but a C
compiler.

Older `devel-32-el`, `devel-64-el`, `prebuilt-32-el`, `prebuilt-64-el`
branches predate the multi-architecture layout.

## Build System

GNU Autotools (autoconf/automake).

```sh
autoreconf --install
./configure
make
make install
```

### Key configure options

- `--enable-64-bit` — Native 64-bit mode (default: yes); use `--disable-64-bit` to force 32-bit
- `--enable-docs` — Build LaTeX documentation (default: yes)
- `--enable-ndebug` — High-speed mode, disables debug tracing (default: yes, sets -DFAST)
- `--enable-threads` — Thread support (default: no, experimental)
- `--with-compile=world|prebuilt|guile|check` — how `.oak` becomes `.oa` (default `check`: first available in that order)
- `--with-cold-link=world|c|guile|check` — how the cold world is linked: `tool.oak` in the bootstrap world, `oak-cold-linker`, or `tool.oak` in the Guile host (default: `world` when compiling with one, else `c`)
- `--with-world[=PATH]` — World image to bootstrap from (default: search `prebuilt/src/world/<arch>/` and installed locations; `no`: none)
- `--with-oaklisp=OAKLISP` — Existing emulator to run the bootstrap world with (default: the one being built)
- `--with-bytecode[=DIR]` — Prebuilt `.oa` directory (default: search `prebuilt/src/world/bc2-64`, then `bc2-32`)
- `--with-guile[=GUILE]` — Guile 3 for the Guile-hosted Oaklisp in `src/cold-compiler/` (default: search)

### Architectures

An Oaklisp architecture is (instructions per ref, word size, byte order):

| Name | Meaning |
|------|---------|
| `bc2-32`, `bc2-64` | bytecode: 2 instructions/ref, 32- or 64-bit refs (byte-order independent) |
| `bc2-el32`, `bc2-eb32`, `bc2-el64`, `bc2-eb64` | world/emulator: little/big endian, 32/64-bit |

These names are used for `prebuilt/src/world/<arch>/`, for `--target`, and in
file header lines (see below). `configure` sets `OAK_HOST_ARCH` (e.g. `bc2-el64`)
and `OAK_BYTECODE_ARCH` (e.g. `bc2-64`). Bytecode compiled for `bc2-32` is
usable on 64-bit systems too (the fixnum range only affects integer constants).

In Oaklisp, `src/world/architecture.oak` defines `host-architecture` (an alist
with keys `word-size`, `instructions-per-ref`, `endian`) and the fluid
`#*target-architecture` which the compiler (assembler) uses; `--target ARCH`
sets it. The cold linkers store `%%word-size` and `%%instructions-per-ref`
in the world, which is how the running system knows its own word size
(`most-negative-fixnum` in bignum.oak is derived from it).

### Bootstrap methods (chosen by configure; automake conditionals `COMPILE_WORLD/PREBUILT/GUILE`, `COLD_LINK_WORLD/C/GUILE`)

- **Compile with a world** (normal dev build): `.oak` → `.oa` with `$(OAK) --world W -- --locale compiler-locale --load assembler --locale system-locale --target ARCH --compile`. The freshly compiled assembler is preloaded so an older bootstrap world can compile sources using new instructions; `multiproc.oa` also preloads `multi-em`, `file-io.oa` preloads `streams` (for `(%stream-primitive 14)`). A new open-coded primitive used from another file needs the same treatment, otherwise the first-pass world calls it generically and fails.
- **Prebuilt bytecode**: `.oa` copied from `prebuilt/`; `system-version.oa` is the prebuilt one with its version string replaced by sed.
- **Guile**: `src/cold-compiler/oak-bootstrap.scm` compiles everything in one run (`guile.stamp`); it hosts the world's own compiler, so output is byte-identical. It has `--target`, `--load`, `--eval`, and a `scheme-locale` with scheme-macros and scheme loaded (scheme.oak is compiled in scheme-locale, against its own definitions). It cannot run `instruction-table.oak`'s `dump-instruction-table`, so `src/emulator/instr-data.c` (debugging builds only) comes from the world just built, a bootstrap world, or `prebuilt/`; `make dist` ships it.
- **Cold link**: `tool.oak` (in the world or under Guile) or `oak-cold-linker`; all three produce byte-identical `.cold` files (symbols laid out in first-seen order).

The compiler reaches a fixpoint: `make check` runs `check-fixpoint`, `check-cold-linkers`, `check-guile-compile` in `src/world`.

### Tests and benchmarks

`make check` runs `tests/*.test` (automake test driver; logs in `tests/*.log`). Test programs `tests/*.oak` print `PASS`/`FAIL` lines; `tests/testlib.sh` has the helpers. `make bench` runs `tests/bench.sh` (`-r N`, `-o file`, `-c old new`). Note that `--load` binds `#*print-length`/`#*print-level`; the test programs reset them.

`make prebuilt` refreshes `prebuilt/` (bytecode as `bc2-32`, this machine's world, instr-data.c, PDFs).

### Important build notes

- **64-bit by default:** On 64-bit platforms, builds natively with 64-bit pointers and 62-bit fixnums. Use `--disable-64-bit` to force 32-bit mode (adds `-m32`).
- **Big-endian works** for both word sizes (tested under qemu-user with s390x and powerpc cross compilers, `./configure --host=s390x-linux-gnu CC=... LDFLAGS=-static`). On 64-bit big-endian, instructions live in the *high* 32 bits of each code ref (first in memory order); see `code-vector.oak`, `fasl.oak`, and `read_ref()` in `worldio.c`.
- **Cold-world files can't contain bignums:** the linkers range-check integer constants against the target fixnum size. Don't write literals ≥ 2^29 in files listed in `COLDFILES`.
- **Shifts are not constant-folded:** `ash-left`/`ash-right`/`rot-*` wrap modulo the fixnum size, so folding them would make compiled code depend on the compiling host's word size. They deliberately lack `foldable-mixin` (numbers.oak).
- **Rest args are not lists:** `(define (f a . rest) ...)` follows the documented Oaklisp semantics (rest args live on the stack; use `listify-args`/`consume-args`; a bare `(rest-length rest)` leaves them on the stack and the compiler warns). A change making `lambda` auto-listify was dropped (twice) because it broke `^super`, `exit`, etc. when the world was recompiled. Exception: `scheme.oak`/`scheme-macros.oak` are compiled in `scheme-locale`, whose `add-method` gives dotted lists their R3RS meaning, so code there must be written R3RS-style.
- **Threads:** `--enable-threads` = `-DTHREADS -DUSE_MARK_SWEEP` (they can't be separated: THREADS without mark-sweep hangs). Experimental: with `--pthreads N` the compiler deadlocks. Costs up to 5× on allocation-heavy code (`format`) even unused; numbers in BUILD.md.

## Architecture

### Two runtime components

1. **Emulator** (`src/emulator/oaklisp`) — C executable, the bytecode VM
2. **World image** (`oakworld.bin`) — Contains the entire Oaklisp runtime, compiler, and standard library

### Emulator key files

| File | Purpose |
|------|---------|
| `oaklisp.c` | Main entry point, arg parsing, world loading, VM start |
| `loop.c` | Core bytecode interpreter loop (~1300 lines) |
| `data.h` | Data types, 2-bit tag scheme, VM registers |
| `gc.c` | Copying garbage collector with locative support |
| `stacks.c/h` | Value and context stacks |
| `worldio.c` | World image load/save |
| `cmdline.c` | Command-line parsing |
| `weak.c` | Weak pointer tables |
| `oak-cold-linker.c` | Standalone C cold world linker (bootstrap tool) |

### Tag scheme (2-bit, low bits)

- `00` = fixnum (62-bit signed on 64-bit, 30-bit on 32-bit)
- `01` = locative (pointer to a single cell)
- `10` = other immediate (characters, etc.)
- `11` = reference to boxed heap object

TAGSIZE is always 2 (number of tag bits). REF_SHIFT = log2(sizeof(ref_t)): 2 on 32-bit, 3 on 64-bit. Pointer/locative tags encode word addresses shifted by REF_SHIFT. On 32-bit, TAGSIZE == REF_SHIFT by coincidence; on 64-bit they differ — code that conflates them will break.

### World build layers

The world is built in stages from `.oak` source files:

1. **Cold world** (`new.cold`) — Core runtime linked from COLDFILESD `.oa` files (kernel, types, reader, evaluator, REPL). Built by `oak-cold-linker` (default) or `tool.oak`.
2. **oakworld-1.bin** — Boot cold world into warm world
3. **oakworld-2.bin** — Load MISCFILES (macros, bignums, rationals, dev tools)
4. **oakworld-3.bin** — Load COMPFILES (compiler) into `compiler-locale`, then EXPORTFILES (`oaklisp-exports.oak`) into `system-locale`
5. **oakworld.bin** — Load RNRSFILES (Scheme compatibility) into `scheme-locale`

### Locales (namespaces)

- `system-locale` — Everything: the language plus its implementation (default for the REPL)
- `compiler-locale` — Compiler internals; inherits `system-locale`
- `oaklisp-locale` — No superiors; the user-level subset of `system-locale`, populated by the explicit list in `src/world/oaklisp-exports.oak` (variables share cells and frozen status, macros share expanders). Since macros aren't hygienic, anything an exported macro expands into must be exported too — the file has a section for that.
- `user-locale` — Inherits `oaklisp-locale` only
- `scheme-locale` — RnRS Scheme compatibility; inherits `system-locale`

## File Extensions

| Extension | Meaning |
|-----------|---------|
| `.oak` | Oaklisp source code |
| `.oa` | Compiled Oaklisp bytecode object |
| `.bin` | Binary world image |
| `.cold` | Cold (unwarmed) world image |
| `.sym` | Symbol table file |

## Coding Conventions

### C (emulator)

- Classic C style
- `e_` prefix for emulator-level global variables and VM registers
- `//` comments for license headers
- GPL license header in every file

### Oaklisp (world)

- `;;;` for file-level comments, `;;` for section comments, `;` for inline
- GPL license header in every file
- `define-instance` for type instances
- `add-method` for method definitions
- `define-syntax` for macros

## Bootstrap Linker (`oak-cold-linker`)

`src/emulator/oak-cold-linker.c` is a standalone C program (~2000 lines) that links compiled `.oa` bytecode files into a cold world image (`.cold`). It replicates the algorithm of `src/world/tool.oak` entirely in C, breaking the circular dependency that normally requires a running Oaklisp to build the cold world. It is used only when bootstrapping from bytecode; when a running Oaklisp is available the build uses `tool.oak` (`(tool-files '(files...) 'new "bc2-64")`). Both linkers produce equivalent (not byte-identical: data is laid out in hash-table order) worlds and must be kept in sync. Header parsing is shared via `src/emulator/oak-header.h` (header-only, so the linker still builds as one translation unit).

### Building and running

```sh
# Build (no dependencies beyond libc; oak-header.h must be alongside)
gcc -O2 -o oak-cold-linker src/emulator/oak-cold-linker.c

# Generate cold world (COLDFILESD list from src/world/Makefile-vars)
cd src/world
../../oak-cold-linker --target bc2-64 -o new cold-booting kernel0 do kernel0types ...

# Then boot as usual
../../src/emulator/oaklisp --world new.cold --dump oakworld-1.bin
```

`--target ARCH` selects the word size (`--64bit`/`--32bit` are accepted abbreviations; default bc2-64). File arguments are basenames; `.oa` is appended automatically. The linker checks each `.oa` header's `instructions-per-ref`, range-checks integer constants against the target fixnum size, and writes `;oaklisp-world format=cold ...` as the first line of the `.cold` file. Its reader follows the Oaklisp reader: `\` quotes the next character in both tokens and strings (no C-style escapes).

### Cold world memory layout

The linker arranges memory in four contiguous regions:

| Region | Start | Contents |
|--------|-------|----------|
| opc-space | 0 | Code blocks (2 opcodes per word) |
| var-space | opc_count/2 | Global variable cells |
| sym-space | var + var_count | Symbol entries (2 words each: type-ptr + name-string) |
| dat-space | sym + sym_count*2 | Strings, pairs, handbuilt type objects |

### .oa file format

Compiled `.oa` files start with an optional header line, then use the "old" format:

```
;oaklisp-bytecode word-size=32 instructions-per-ref=2
(SYMBOL-LIST (BLOCK1 BLOCK2 ...))
```

- **SYMBOL-LIST**: `(sym1 sym2 ...)` or `()` — shared symbol vector for the file
- **BLOCK**: `(FLAT-PATCHES OPCODES)` where:
  - **FLAT-PATCHES**: interleaved triples `(type offset value ...)`:
    - `type >= 5`: symbol reference (actual-type = type-5, value = SYMBOL-LIST[index])
    - `type < 5`: literal (0=variable, 1=code, 2=constant)
  - **OPCODES**: flat list of 16-bit instruction numbers (must be even count)
- Constant values in patches can be: symbols, nil `()`, `#t`, numbers, characters `#\X`, pairs, strings
- Special syntax: `#[symbol ""]` for the empty-named symbol; `#\COERCER` (char 25), `#\FLUID` (char 22)

### .cold file format

```
;oaklisp-world format=cold word-size=64 instructions-per-ref=2
VSTKSIZE CSTKSIZE BOOTMETHOD WORLDSIZE    (4 hex values, header)

 HEX HEX ^HI16LO16 HEX ...               (8 values per line)
...
0                                           (empty weak pointer table)
```

- Plain numbers: ` ` prefix + uppercase hex
- Opcode pairs: `^` prefix + hex(hi16) + zero-padded-4-digit-hex(lo16)
- The emulator's `read_ref()` in `worldio.c` swaps the two 16-bit halves of `^`-prefixed values on little-endian machines, and on 64-bit big-endian machines shifts them into the high 32 bits, so the first opcode always comes first in memory

Binary worlds (`.bin`) start with `;oaklisp-world format=binary endian=little word-size=64 instructions-per-ref=2\n` followed by raw refs; the emulator refuses worlds whose header doesn't match it. Legacy binary worlds start with four `\002` (32-bit) or `\004` (64-bit) bytes.

### Tag encoding in the cold world

```
tagize_int(x) = x * 4                           (INT_TAG=0)
tagize_imm(x) = 1 + x * 4                       (IMM_TAG=1)
tagize_loc(x) = 2 + (x << ref_shift)            (LOC_TAG=2)
tagize_ptr(x) = 3 + (x << ref_shift)            (PTR_TAG=3)
```

Where `ref_shift` is 2 for 32-bit, 3 for 64-bit.

### Key constants (from tool.oak)

| Constant | Value | Meaning |
|----------|-------|---------|
| REG_CODE_DELTA | 4 | Words before each regular block (type-ptr, size, opcodes...) |
| TOP_CODE_DELTA | -2 | Words before top-level block (return→noop chain) |
| RETURN_OPCODE | 6144 (24*256) | Return instruction bytecode |
| COERCABLE_TYPE_SIZE | 10 | Words for string type object |
| TYPE_SIZE | 9 | Words for cons-pair/code-vector type objects |
| CHARS_PER_WORD | 3 | Characters packed per word in string objects |

### String packing

Strings are stored as: `[type-ptr, total-word-count, char-count, packed-chars...]`. Characters are packed 3 per word, low byte first: `c0 | (c1 << 8) | (c2 << 16)`. Total size = `3 + ceil(strlen / 3)`.

## Key Technical Constraints

- **No floating point** — Rationals are used instead
- **No FFI** — No foreign function interface for calling C from Oaklisp
- **2 instructions per ref** — 16-bit bytecodes packed two per ref cell (32- or 64-bit); on 64-bit, the other 32 bits of each cell are unused (the low half on big-endian). The assembler is parameterized on `instructions-per-ref` of `#*target-architecture`; `fasl.oak`, `code-vector.oak`, `tool.oak`, and the emulator assume 2 and would need work for a 4-instruction packing.
- **`Makefile-vars`** in `src/world/` is auto-generated by `make-makefile.oak` from `files.oak`; regenerate with `make Makefile-vars` after changing `files.oak`. Contains `COLDFILESD` (with interleaved marker files `st`, `da`, `pl`, `do`, `em`), `MISCFILES`, `COMPFILES`, `RNRSFILES`.
- **`system-version.oak`** is generated from `system-version.oak.in` by configure (a bytecode bootstrap has no compiler for it and patches the version string into the prebuilt `system-version.oa` instead)

## Running Oaklisp

```sh
# Basic invocation
oaklisp --world path/to/oakworld.bin -- [oaklisp-options]

# Emulator options (before --)
--world FILE       Load world image
--dump FILE        Dump world image on exit
--size-heap N      Set heap size
--trace-gc         Trace garbage collection

# Oaklisp options (after --)
--eval EXPR        Evaluate expression
--load FILE        Load file
--compile FILE     Compile file
--target ARCH      Compile for architecture ARCH (bc2-32, bc2-64)
--locale LOCALE    Set current locale
--exit             Exit after processing
--help             Show help
```

Environment variable `OAKWORLD` overrides the default world location.

## Testing

- Unit testing framework at `src/misc/unit-testing.oak`
- Example tests at `src/misc/testing-tests.oak` and `doc/examples/test-bank-example.oak`
- Benchmarks: `src/world/compile-bench.oak`, `src/world/tak.oak`
- No automated CI/CD pipeline

```sh
# Run unit tests
oaklisp --world oakworld.bin -- \
  --load src/misc/unit-testing --load src/misc/testing-tests \
  --eval "(run-all-tests unit-tests)" --exit

# Quick compilation test
cd src/world && oaklisp --world oakworld.bin -- --compile sort --exit
```

## Debian Packaging

Two binary packages:
- **oaklisp** — Emulator binary + oakworld.bin + man page
- **oaklisp-doc** — PDF documentation + examples

Build with: `dpkg-buildpackage` or `debuild` (requires `gcc-multilib` for 32-bit mode on 64-bit hosts, `texlive` + `latexmk` for docs).

## Git Branches

- `devel` — Development branch; source only (no `prebuilt/`, no `m4/`)
- `master` — `devel` plus `prebuilt/` and `m4/`, refreshed after each merge from `devel`
- `pristine-tar` — Debian pristine-tar data
