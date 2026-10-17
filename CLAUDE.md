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
resources/          Desktop entry, icon, shell completions (bash, zsh, fish)
prebuilt/           Prebuilt bootstrap artifacts, on the "master" branch only
m4/                 Cached AX_* autoconf macros, on the "master" branch only
debian/             Debian packaging
```

### Branches

- `main` — development: source only, never `prebuilt/` or `m4/`
- `prebuilt` — `main` plus the generated material: `prebuilt/` (bytecode,
  world images, PDFs, instr-data.c) and the cached `m4/` macros, as appropriate.
- `pristine-tar` — Debian pristine-tar data

The rule: commit source changes on `main`; every so often merge `main`
into `prebuilt` and follow the merge with a commit that refreshes
`prebuilt/` (`make prebuilt`).  Commit messages on `main` don't discuss
prebuilt content; that goes in the refresh commit on `prebuilt`.

`main` and release tarballs (`make dist` deliberately omits `prebuilt/`)
build from source alone with Guile 3 (`build-from-guile`, which make
picks when there is no old world and no prebuilt bytecode).  `main`
builds with nothing but a C compiler.  `make rebuild-world` saves the
world just built (`src/world/old-oakworld.bin`, or `OLD_WORLD=PATH`),
runs `clean-world`, and rebuilds all objects and the world from scratch
with it (`build-from-old-world`); `make bootstrap` = build + two
rebuilds + `check-fixpoint`.  The
Debian package builds that way: Guile, then `make bootstrap`.

## Build System

GNU Autotools (autoconf/automake).

```sh
autoreconf --install
./configure
make
make install
```

### Key configure options

- `--enable-64-bit` — native pointer size (default: yes); `--disable-64-bit` builds a 32-bit executable on a 64-bit machine with the compiler option for its 32-bit memory model (`-m32` etc.)
- `--enable-docs` — Build LaTeX documentation (default: yes)
- `--enable-ndebug` — High-speed mode, disables debug tracing (default: yes, sets `-DFAST`)
- `--enable-threads` — Thread support (default: no, experimental)
- `--enable-cold-linker` — Build `oak-cold-linker`, the C reimplementation of `tool.oak` (default: no)
- `--with-instructions-per-ref=2|4` — 16-bit instructions packed per code ref: 4 (default) with 64-bit refs, 2 with 32-bit refs (or by choice on 64-bit, for bytecode 32-bit machines can load); sets `INSTRS_PER_REF` in config.h
- `--with-guile[=GUILE]` — Guile 3 for the Guile-hosted Oaklisp in `src/cold-compiler/` (default: search)
- `--with-readline` — line editing and history on an interactive standard input with GNU readline (default: if found; `src/emulator/lineedit.c`, `--no-line-editing` at run time)

How the world is bootstrapepd is not a configuration decision, so configure.ac does not case off on it. That decision is made at built-time by make (see Bootstrap methods).

### Architectures

An Oaklisp architecture is (instructions per ref, word size, byte order):

| Name | Meaning |
|------|---------|
| `bc2-32`, `bc2-64`, `bc4-64` | bytecode: 2 or 4 instructions/ref, 32- or 64-bit refs (byte-order independent; 4 needs 64-bit refs) |
| `bc2-el32`, `bc2-eb32`, `bc2-el64`, `bc2-eb64`, `bc4-el64`, `bc4-eb64` | world/emulator: little/big endian, 32/64-bit, 2 or 4 instructions/ref |

These names are used for `prebuilt/src/world/<arch>/`, for `--target`, and in
file header lines (see below). `configure` sets `OAK_HOST_ARCH` (e.g. `bc4-el64`)
and `OAK_BYTECODE_ARCH` (e.g. `bc4-64`). Bytecode compiled for `bc2-32` is
usable on 64-bit systems too (the fixnum range only affects integer constants).

In Oaklisp, `src/world/architecture.oak` defines `host-architecture` (an alist
with keys `word-size`, `instructions-per-ref`, `endian`) and the fluid
`#*target-architecture` which the compiler (assembler) uses; `--target ARCH`
sets it. The cold linker stores `%%word-size` and `%%instructions-per-ref`
in the world, which is how the running system knows its own word size
(`most-negative-fixnum` in bignum.oak is derived from it).

### Bootstrap methods (decided by make in `src/world/Makefile.am`, from what exists when it runs)

Targets: `build-from-old-world` (compile and `tool.oak`-link with `$(OAKLISP)` = the emulator just built running `$(OLD_WORLD)`), `build-from-bytecode` (copy `$(PREBUILT_BYTECODE_DIR)`, link with `tool.oak` under Guile), `build-from-guile` (compile and link under Guile). Plain `make` picks the first possible: `OLD_WORLD` exists (default: `src/world/old-oakworld.bin` if it exists, else `prebuilt/src/world/<arch>/oakworld.bin`), else, with Guile, prebuilt bytecode, else Guile alone. Internally `WORLD_METHOD` sets `OAK_COMPILE` = world|prebuilt|guile and `OAK_COLD_LINK` = world|guile, both overridable on the make command line; the rules dispatch through `OA_PREREQ_$(OAK_COMPILE)`, `OA_COMPILE_$(OAK_COMPILE)`, `LINK_COLD_$(OAK_COLD_LINK)`. Make never looks for an installed Oaklisp; name one with `OLD_WORLD=` (and `OAKLISP=` if the emulator being built can't run it, e.g. another word size).

- **Compile with a world** (normal dev build): `.oak` → `.oa` with `$(OAK) --world $(OLD_WORLD) -- --locale compiler-locale --load assembler --locale system-locale --target ARCH --outdir . --compile $(srcdir)/NAME` (`--outdir` sets the fluid `#*compiler-output-directory`; `compile-file` also takes an optional output name; a world older than `--outdir` can't be `OLD_WORLD`, and `build-from-old-world` says so). The freshly compiled assembler is preloaded so an older bootstrap world can compile sources using new instructions; `multiproc.oa` also preloads `multi-em`, `file-io.oa` preloads `streams` (for `(%stream-primitive 14)`). A new open-coded primitive used from another file needs the same treatment, otherwise the first-pass world calls it generically and fails.
- **Prebuilt bytecode**: `.oa` copied from `prebuilt/`, `system-version.oa` included, so the world reports the version the bytecode was compiled with until rebuilt with itself (the copy warns).
- **Guile**: `src/cold-compiler/oak-bootstrap.scm` compiles everything in one run (`guile.stamp`); it hosts the world's own compiler, so output is byte-identical. It has `--target`, `--load`, `--eval`, and a `scheme-locale` with scheme-macros and scheme loaded (scheme.oak is compiled in scheme-locale, against its own definitions). It cannot run `instruction-table.oak`'s `dump-instruction-table`, so `src/emulator/instr-data.c` (debugging builds only) comes from the world just built, an old world, or `prebuilt/`; `make dist` ships it.
- **Cold link**: `tool.oak`, in the old world or under Guile; both produce byte-identical `.cold` files (symbols laid out in first-seen order).

The compiler reaches a fixpoint: `make check` runs `check-fixpoint`, `check-cold-linkers`, `check-guile-compile` in `src/world`.

### Tests and benchmarks

`make check` runs `tests/*.test` (automake test driver; logs in `tests/*.log`). Test programs `tests/*.oak` print `PASS`/`FAIL` lines; `tests/testlib.sh` has the helpers. `make bench` runs `tests/bench.sh` (`-r N`, `-o file`, `-c old new`; `-n` compiles the benchmarks natively, `-x` builds them as stand-alone executables with oakc, `-w` as whole-program executables, so all four compare: bytecode, mixed native, separate and whole-program compilation). Note that `--load` binds `#*print-length`/`#*print-level`; the test programs reset them. `make -C src/world check-native-fixpoint` (run by consistency.test when the native world exists) recompiles every source with the native world's compiler and demands byte-identical objects.

`make prebuilt` refreshes `prebuilt/` (bytecode as `bc2-32` or `bc4-64` per this system's packing, this machine's world, instr-data.c, PDFs).

### Important build notes

- **64-bit by default:** On 64-bit platforms, builds natively with 64-bit pointers and 62-bit fixnums. Use `--disable-64-bit` to force 32-bit mode (adds `-m32`).
- **Big-endian works** for both word sizes, native code included (tested under qemu-user; zig makes a no-install cross compiler: `./configure --host=s390x-linux-gnu CC="zig cc -target s390x-linux-musl -std=gnu23 -D_GNU_SOURCE" LDFLAGS=-static`, world built from the host's `.oa` files, and `tests/qemu-native.sh` runs natively compiled programs statically linked into an emulator-carrying executable, since a static binary cannot dlopen). On 64-bit big-endian, instructions live in the *high* 32 bits of each code ref (first in memory order); see `code-vector.oak`, `fasl.oak`, and `read_ref()` in `worldio.c`.
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
| `oak-cold-linker.c` | C reimplementation of the cold linker (optional, `--enable-cold-linker`) |
| `cmdline.c` | Command-line parsing |
| `weak.c` | Weak pointer tables |
| `native.c`, `oak-native.h` | Natively compiled code: module loading, the NATIVE instruction's support, the header generated C includes |

### Tag scheme (2-bit, low bits)

- `00` = fixnum (62-bit signed on 64-bit, 30-bit on 32-bit)
- `01` = locative (pointer to a single cell)
- `10` = other immediate (characters, etc.)
- `11` = reference to boxed heap object

TAGSIZE is always 2 (number of tag bits). REF_SHIFT = log2(sizeof(ref_t)): 2 on 32-bit, 3 on 64-bit. Pointer/locative tags encode word addresses shifted by REF_SHIFT. On 32-bit, TAGSIZE == REF_SHIFT by coincidence; on 64-bit they differ — code that conflates them will break.

### World build layers

The world is built in stages from `.oak` source files:

1. **Cold world** (`new.cold`) — Core runtime linked from COLDFILESD `.oa` files (kernel, types, reader, evaluator, REPL). Linked by `tool.oak`, in the old world or under Guile.
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

## Cold Linker (`tool.oak`)

`src/world/tool.oak` links compiled `.oa` bytecode files into a cold world image (`.cold`): `(tool-files '(files...) 'new "bc2-64")`, the target naming the word size. It runs in an Oaklisp world (`build-from-old-world`) or, when there is none, in the Guile-hosted Oaklisp (`build-from-bytecode`, `build-from-guile`; `src/cold-compiler/oak-bootstrap.scm --load tool --eval ...`); both produce byte-identical cold worlds, which `check-cold-linkers` verifies. It is the linker the build uses; `src/emulator/oak-cold-linker.c` is a C reimplementation (~2000 lines, built only with `configure --enable-cold-linker`, used with `make OAK_COLD_LINK=c`) kept as an independent check and for bootstrapping where nothing can run `tool.oak`; it must be kept in sync with `tool.oak` by hand, and `check-cold-linkers` compares it when it is built. It checks each `.oa` header's `instructions-per-ref`, range-checks integer constants against the target fixnum size, and writes `;oaklisp-world format=cold ...` as the first line of the `.cold` file. Symbols are laid out in the order they are first seen (not hash-table order), so the output does not depend on the hash function. The emulator's side of the header formats is `src/emulator/oak-header.h`.

### Cold world memory layout

tool.oak arranges memory in four contiguous regions:

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
- `^`-prefixed values hold the word's `instructions-per-ref` opcodes as one number, first opcode most significant (`^HHHHLLLL` for bc2, four fields for bc4); the emulator's `read_ref()` in `worldio.c` reverses the 16-bit fields on little-endian machines, and on big-endian machines shifts them to the top of the word, so the first opcode always comes first in memory

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
| chars per word | 3 / 7 | Characters packed per word in string objects (32- / 64-bit refs) |

### String packing

Strings are stored as: `[type-ptr, total-word-count, char-count, packed-chars...]`. Characters are packed low byte first into the fixnum of each word, as many as fit: 3 with 32-bit refs (`c0 | c1<<8 | c2<<16`), 7 with 64-bit. Total size = `3 + ceil(strlen / chars-per-word)`. `%chars-per-word` in strings.oak is a variable derived from `%%word-size`, not a constant, so objects are word-size independent; the linkers (`target-chars-per-word`, `CHARS_PER_WORD`) and `oak_c_string_fill` know the same rule.

## Native Compiler (`native-compiler.oak`)

Mixed mode, milestones 1-2 of `CompilerPlan.md`.  `--compile-native
FILE` (`native-compile-file`, in compiler-locale) compiles as
`--compile` does up to the peephole-optimized instruction lists, then
translates every code block (top-level code included) instruction by
instruction into a C function in `FILE.c`, built with the C compiler
(`(%stream-primitive 16)`, `oak_native_cc`) into `FILE.so`.  `FILE.oa`
is ordinary bytecode with a second header line `;oaklisp-native
FILE.so`, which `load-oa-file` loads (`(%stream-primitive 15)`,
dlopen) relative to the .oa.  The stub layout is described at the top
of `oak-native.h`.

- A translated block's code vector is a stub: `NATIVE k` instructions
  (opcodes 35-42: opcode 35 + k/256, argument k%256, up to 2048 resume
  points), escapes, operands, constants, then the C function's name
  and the index/epoch the emulator caches its lookup in.
- An instruction without C (an *escape*) stays in the stub as bytecode
  followed by a NATIVE instruction; the C hands it to the emulator, so
  it runs, traps included, exactly as in bytecode.  LOAD-BP, STORE-BP
  and MAKE-BP-LOC read their offsets from copies kept in the stub
  (*operands*), which `remap-your-ivars` rewrites as it would bytecode.
- Generated code keeps the emulator's value and context stacks; values
  live across calls only there, so the GC, `call/cc` and stack flushing
  need nothing new.  A non-tail call pushes a context frame returning
  to the NATIVE instruction of the next resume point.  Calls are
  dispatched in C (lambda, or the operation's method cache /
  `oak_native_find_method`); native-to-native calls and returns jump
  directly with `musttail` where the C compiler has it, otherwise via
  the emulator (`-DOAK_NATIVE_TRAMPOLINE` forces that).
- A call whose operation was just pushed from a global (milestone 5)
  gets a per-call-site cache (`oak_native_site`, `ON_SITE_CALL` in
  `oak-native.h`): operation → method, code, environment, C function,
  jumped to directly on a hit.  A lambda site needs only the operation
  compare (the lambda slot is never written); a generic site also
  caches the first argument's type and bp offset, witnessed by the
  operation's own one-entry cache, which `%INSTALL-METHOD-WITH-ENV`
  parks -- so ADD-METHOD needs no epoch, and a site refills only on
  that witness (type-alternating sites cost compares, never fills).
  Sites hold raw refs, so every GC, world load and module registration
  flushes them all (`oak_native_flush_sites`); a redefined global
  misses on the operation compare and refills.
- The emulator links with `-rdynamic` so modules see its symbols, and
  passes the generated code its own `CC`/`CFLAGS` and `-I` this build
  tree (env `OAKLISP_NATIVE_CC`, `OAKLISP_NATIVE_CFLAGS`,
  `OAKLISP_NATIVE_INCLUDE` override); headers are not installed yet.
- Function names are the file name plus a hash of the C, since modules
  share one name space in the emulator.
- Tests: `native.test`; `native-parity.test` compiles every
  `tests/*.oak` both ways and compares the output; `standalone.test`
  builds and runs stand-alone executables.
- Not with `--enable-threads`.  `load-file.oak` and
  `native-compiler.oak` need the new `streams.oa` preloaded when an
  older world compiles them (as `file-io.oak` does); `warm.oak`,
  `fasl.oak` and `load-file.oak` need the new `subprimitive.oa` for
  `%embedded-program-char` (stream primitive 17: a character of the
  embedded program, or with index -1 whether one is embedded, which is
  how warm boot and the loader keep quiet in an executable).

### Stand-alone executables (`oakc`, milestones 3-4): straight C

`./oakc prog.oak... [-o prog]` (top level, generated by configure, runs
out of the build tree) builds an executable that is C through and
through: no interpreter is linked in (`nm PROG` shows no `loop`), no
bytecode runs, and nothing is loaded at startup.  It needs the native
runtime: `make -C src/world native-world` first.

- Every source is compiled to C in strict mode (`#*native-strict?`:
  an instruction without a C translation is an error; there are none
  left - the full instruction set is translated, `native-ops.c`
  holding the C of the big ones and `dispatch.c` the method search,
  both shared with the interpreter).
- At build time `bake-file` (load-file.oak) links each compiled file
  into the native world without running it, collecting one operation
  per file in `*baked-program*`, and the world is dumped; the
  generated main.c carries that image as data and enters
  `oak_main_native` (embed-native.c), whose engine is the native call
  driver `oak_native_run` (native-ops.c) - a small funcall/trap/signal
  dispatcher, not an interpreter.  At run time `(run-baked-program)`
  runs the files in order.
- Preemption works in compiled code: `ON_PENDING`/`ON_POLL`
  (oak-native.h) count the shared alarm clock (`oak_timer_*`, data.c)
  that ENABLE-ALARMS drives, and the driver (and the emulator's NATIVE
  case) deliver alarm traps exactly like the interpreter; the thread
  system (multiproc.oak) depends on it.
- Runtime `eval`/`compile` of non-trivial forms needs bytecode, which
  such an executable cannot run ("bytecode reached" is the error);
  simple `--eval` forms are interpreted and work.  The escape hatch is
  `--interpreter`: the same baked world and native modules, entered
  through the emulator (`oak_main_embedded`), so evaled code runs in
  the interpreter while everything compiled stays native.  Plain oakc
  warns, naming the global, when a program's objects mention `eval`,
  `compile` or `compile-file`.
- `--strip` is dead code removal: `strip-locales` (load-file.oak)
  empties the locales before the bake dump, keeping only
  RUN-BAKED-PROGRAM and the globals the `--eval` forms name, the
  pre-dump GC drops what nothing reaches, and runtime modules whose
  stubs did not survive are left out of the link (oakc reads the
  surviving function names out of the image).  Nothing can be looked
  up by name afterwards, so it excludes `--interpreter`.
- `-w`/`--whole-program`: each file is compiled twice -- summaries
  (`.ois`: defines with arity and inferred result type, assigns,
  escapes) first, then codegen with every summary in hand.  Calls
  resolve statically: same-file always (guarded by the callee's code
  vector, shared in the .oa by the assembler's block memoization),
  cross-file and into the runtime in executables (extern functions,
  guarded by operation pins a generated link-time prelude binds), and
  type-proven generic calls through `%method-for`/`%bp-offset-for`
  (closed world assumed for those).  Redefinition misses the guards
  and falls back.  Small single-definition never-assigned functions
  inline across files (their bodies travel in the `.ois`); the
  abstract-stack inference drops tag checks on proven operands,
  cross-file via the summaries' result types, and fuses a comparison
  into the branch right after it (`EQ?` always, others when proven),
  in every mode.
- `--ir` turns on the C-locals lowering: within trap-free,
  allocation-free runs the stack top lives in C locals (`v0..v7`),
  spilled with plain stores into room reserved while no local was live
  (`ON_RESERVE`), so no collection can find a ref only a C variable
  holds; everything else spills first and uses the ordinary emission.
- Options: `-o`, repeatable `--eval EXPR` (after the program runs),
  `--locale`, `--runtime DIR`, `-w`/`--whole-program`, `--ir`,
  `--interpreter`, `--strip`, `--keep`.  The executable's own
  arguments follow a `--` on the argument line.  Sizes: ~7 MB
  stripped, ~9 MB with the full runtime linked.

`make native-world` (in `src/world`, needs the ordinary world first)
rebuilds the whole world natively into `src/world/native/`: stub `.oa`
files (top-level blocks stay bytecode for the cold linker's
return-to-noop chaining), a world image built by the normal
cold-link/boot stages run with `--native-module` for every `.so`, and
`liboaknat.a` + `oak-runtime-modules.h` for oakc.  Compiler files
compile in compiler-locale, scheme files in scheme-locale, like
`COMPILE_ALL`.  The emulator can run that world too, given the
`--native-module` flags (mixed mode, used by the build itself).

Debugging aids: `OAK_RT_TRACE=1` logs the control helpers (throw,
continuations) with stack heights; `--trace-traps` also logs native
traps (`NTRAP ...`) in non-FAST builds; `#*native-exclusions` (a list
of instruction names, or `(all)`) makes the translator escape those
instructions to the emulator - a bisection aid for mixed mode only.

One multiproc fix came out of this (`force` in multiproc.oak): a
waiter left in the scheduler queue and resumed later runs the next
fresh thunk on its own dead stack extent, so a throw out of a forced
promise body unwinds into garbage; `force` now drains the queue before
suspending.  Stock bytecode almost never hit it because alarm
preemption drained the queue first.

## Key Technical Constraints

- **No floating point** — Rationals are used instead
- **No FFI** — No foreign function interface for calling C from Oaklisp
- **2 or 4 instructions per ref** — 16-bit bytecodes packed `instructions-per-ref` per ref cell, in memory order (instruction k at bits 16k on little-endian, at the top of the word on big-endian); with bc2 on 64-bit the other 32 bits are unused. The assembler pads inline refs to a ref boundary per `#*target-architecture`; `code-vector.oak` (`%pack-instructions`/`%unpack-instruction`, `nth`, `remap-your-ivars`), `fasl.oak` and `tool.oak` use `%%instructions-per-ref`/`target-instructions-per-ref`; the emulator uses `INSTRS_PER_REF` (config.h) and `INSTR_STRIDE` (slots per ref). Code that runs in the *host* during compilation (`compiler-eval`, macro expanders, the preloaded `old/*.oa`) is always assembled for the host.
- **`Makefile-vars`** in `src/world/` is auto-generated by `make-makefile.oak` from `files.oak`; regenerate with `make Makefile-vars` after changing `files.oak`. Contains `COLDFILESD` (with interleaved marker files `st`, `da`, `pl`, `do`, `em`), `MISCFILES`, `COMPFILES`, `RNRSFILES`.
- **`system-version.oak`** is generated from `system-version.oak.in` by configure (a bytecode bootstrap cannot compile it and uses the prebuilt `system-version.oa` as it is)

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
--target ARCH      Compile for architecture ARCH (bc2-32, bc2-64, bc4-64)
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

- `main` — Development branch; source only (no `prebuilt/`, no `m4/`)
- `prebuilt` — `main` plus `prebuilt/` and `m4/`, refreshed after each merge from `main`
- `pristine-tar` — Debian pristine-tar data
