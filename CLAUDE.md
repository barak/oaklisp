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
prebuilt/           Prebuilt bootstrap artifacts (worlds, PDFs, instr-data.c)
debian/             Debian packaging
m4/                 Autoconf macros
```

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
- `--with-world=PATH` — Path to oakworld.bin for recompiling .oa files from source
- `--with-oaklisp=OAKLISP` — Path to existing oaklisp executable for bootstrapping

### Important build notes

- **64-bit by default:** On 64-bit platforms, builds natively with 64-bit pointers and 62-bit fixnums. Use `--disable-64-bit` to force 32-bit mode (adds `-m32`).
- **Endian- and word-size-sensitive:** Binary world images (`.bin`) are not portable across endianness or word size, but prebuilt `.oa` files are architecture-independent.
- **Source-only bootstrap:** No prebuilt `oakworld.bin` is needed. The build uses `oak-cold-linker` (built from C alongside the emulator) to link prebuilt `.oa` files from `prebuilt/src/world/` into a cold world. For development, `.oak` files are recompiled from source if a working Oaklisp is available, falling back to prebuilt `.oa` copies otherwise.

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
4. **oakworld-3.bin** — Load COMPFILES (compiler) into `compiler-locale`
5. **oakworld.bin** — Load RNRSFILES (Scheme compatibility) into `scheme-locale`

### Locales (namespaces)

- `system-locale` — Default runtime
- `compiler-locale` — Compiler internals
- `scheme-locale` — RnRS Scheme compatibility
- `user-locale` — Interactive use

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

`src/emulator/oak-cold-linker.c` is a standalone C program (~1200 lines) that links compiled `.oa` bytecode files into a cold world image (`.cold`). It replicates the algorithm of `src/world/tool.oak` entirely in C, breaking the circular dependency that normally requires a running Oaklisp to build the cold world.

### Building and running

```sh
# Build (no dependencies beyond libc)
gcc -O2 -o oak-cold-linker src/emulator/oak-cold-linker.c

# Generate cold world (COLDFILESD list from src/world/Makefile-vars)
cd src/world
../../oak-cold-linker --64bit -o new cold-booting kernel0 do kernel0types ...

# Then boot as usual
../../src/emulator/oaklisp --world new.cold --dump oakworld-1.bin
```

The `--64bit` flag (default) produces 64-bit worlds; `--32bit` produces 32-bit worlds. File arguments are basenames; `.oa` is appended automatically.

### Cold world memory layout

The linker arranges memory in four contiguous regions:

| Region | Start | Contents |
|--------|-------|----------|
| opc-space | 0 | Code blocks (2 opcodes per word) |
| var-space | opc_count/2 | Global variable cells |
| sym-space | var + var_count | Symbol entries (2 words each: type-ptr + name-string) |
| dat-space | sym + sym_count*2 | Strings, pairs, handbuilt type objects |

### .oa file format

Compiled `.oa` files use the "old" format:

```
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
VSTKSIZE CSTKSIZE BOOTMETHOD WORLDSIZE    (4 hex values, header)

 HEX HEX ^HI16LO16 HEX ...               (8 values per line)
...
0                                           (empty weak pointer table)
```

- Plain numbers: ` ` prefix + uppercase hex
- Opcode pairs: `^` prefix + hex(hi16) + zero-padded-4-digit-hex(lo16)
- The emulator's `read_ref()` in `worldio.c` swaps the two 16-bit halves of `^`-prefixed values on little-endian machines

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
- **2 instructions per ref** — 16-bit bytecodes packed two per ref cell (32- or 64-bit); on 64-bit, upper 32 bits of each cell are unused
- **`Makefile-vars`** in `src/world/` is auto-generated by `make-makefile.oak`; do not edit by hand. Contains `COLDFILESD` (194 entries with interleaved marker files `st`, `da`, `pl`, `do`, `em`), `MISCFILES`, `COMPFILES`, `RNRSFILES`.
- **`system-version.oak`** is generated from `system-version.oak.in` by configure

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

- `master` — Main release branch
- `devel` — Development branch (merged into master)
- `pristine-tar` — Debian pristine-tar data
