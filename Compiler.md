# Using the Oaklisp Native Compiler

Oaklisp source normally compiles to bytecode that the emulator runs.
The native compiler turns that same source into C, giving two ways to
run it: *mixed mode*, where compiled files load into the ordinary
running world, and *stand-alone executables* built by `oakc`, which
are C through and through — no interpreter linked in, nothing loaded
at startup.  (`CompilerPlan.md` has the design; this file is just how
to drive it.)

Everything below runs out of a built tree (`./configure && make`);
the generated code includes this tree's headers, which are not
installed.

## Mixed mode: native code in the running world

Compile a file natively instead of to plain bytecode:

```sh
oaklisp --world src/world/oakworld.bin -- --compile-native myfile --exit
```

That writes `myfile.oa` (stub bytecode) and `myfile.so` (the C,
compiled) beside it.  Loading the `.oa` the ordinary way picks the
`.so` up automatically:

```sh
oaklisp --world src/world/oakworld.bin -- --load myfile --exit
```

Everything behaves exactly as the bytecode would — traps, `call/cc`,
redefinition, the debugger — just faster.  A file compiled both ways
prints byte-identical output (`tests/native-parity.test` holds the
whole test suite to that).

The compiler of the generated C defaults to this tree's `CC`; override
with the environment variables `OAKLISP_NATIVE_CC`,
`OAKLISP_NATIVE_CFLAGS` and `OAKLISP_NATIVE_INCLUDE`.

## The native world

```sh
make -C src/world native-world
```

rebuilds the entire world natively into `src/world/native/`: a world
image whose code is all C, the `.so` modules beside it, and
`liboaknat.a` for the executables below.  The emulator can run it in
mixed mode, given the modules:

```sh
cd src/world/native
oaklisp `for m in *.so; do echo --native-module $m; done` --world oakworld.bin
```

`oakc` needs this build; it is not made by default.

## Stand-alone executables: oakc

```sh
./oakc prog.oak -o prog
./prog
```

builds a self-contained executable: the program and the whole runtime
as native code, the world image compiled in as data, no interpreter
(`nm prog` shows no `loop`), nothing read from disk at startup, and
`OAKWORLD` ignored.  Several sources compile separately and load in
order:

```sh
./oakc a.oak b.oak -o prog
```

The executable's own command-line arguments go after a `--`:

```sh
./prog -- these words reach the program
```

Options:

- `-o PROG` — name the output (default: the first source's name).
- `--eval EXPR` — evaluate EXPR after the program's top levels run;
  repeatable, runs in order.  Simple forms only, unless
  `--interpreter` is given (below).
- `--locale NAME` — compile and run in this locale (default
  `system-locale`).
- `--runtime DIR` — the native runtime (default: this tree's
  `src/world/native`).
- `-w`, `--whole-program` — whole-program mode: every file is first
  summarized (`.ois`), then recompiled with all the summaries in
  hand.  Calls resolve statically across files and into the runtime,
  small functions inline across files, and type inference flows
  cross-file.  Every static call is guarded, so a redefined global
  falls back to the generic path; only the resolution of *generic*
  operations assumes the closed world this flag declares — no new
  methods at run time on the operations it resolves.
- `--ir` — the C-locals lowering: within trap-free stretches, values
  live in C variables instead of the value stack.  Opt-in; combines
  with `-w`.
- `--interpreter` — link the bytecode interpreter in.  Run-time `eval`
  or `compile` of non-trivial forms produces bytecode, which a
  straight-C executable refuses ("bytecode reached"); with this flag
  the same baked world runs under the emulator instead, so evaled code
  works while everything compiled stays native.  oakc warns, naming
  the global, when a program mentions `eval`, `compile` or
  `compile-file` without this flag.
- `--strip` — dead code removal: the locales are emptied before the
  world is dumped, the collector drops everything the program cannot
  reach, and runtime modules whose code nothing surviving names stay
  out of the link.  Afterwards nothing can be looked up by name, so it
  excludes `--interpreter`, and `--eval` is limited to the globals it
  names (oakc keeps those).
- `--keep` — keep the work directory and say where it is.

Sizes run about 7 MB stripped, 9 MB with the full runtime; `--strip`
takes roughly another 10–15% off and shrinks the image about 25%.

## Choosing a mode

- Developing and debugging: the ordinary bytecode world, or mixed mode
  when speed matters — everything stays inspectable at the REPL.
- Shipping a program: `oakc prog.oak -o prog`, plus `-w` for the
  cross-file optimizations, plus `--ir` for the lowering, plus
  `--strip` if it needs no run-time name lookups.
- Programs that eval at run time: add `--interpreter` (and skip
  `--strip`).

## Benchmarks and tests

```sh
make bench                       # bytecode
make bench BENCHFLAGS=-n         # mixed native
make bench BENCHFLAGS=-x         # oakc executables
make bench BENCHFLAGS=-w         # whole-program executables
make bench BENCHFLAGS=-i         # lowered executables
```

(`bench-eval` fails by design in executables: run-time eval needs
`--interpreter`.)

### Rough timings

`make bench` best-of-5 CPU milliseconds on one machine (x86-64,
i9-14900KF, gcc 16.2, 2026-10-16); the speedup column is bytecode
over the executable:

| benchmark | bytecode | mixed (`-n`) | executable (`-x`) | speedup |
|-----------|---------:|-------------:|------------------:|--------:|
| bignum    |       50 |           52 |                20 |    2.5× |
| compile   |       26 |           26 |                13 |    2.0× |
| cons      |      116 |           63 |                64 |    1.8× |
| dispatch  |      128 |           83 |                53 |    2.4× |
| fib       |       60 |           18 |                17 |    3.5× |
| format    |      111 |          112 |                44 |    2.5× |
| hash      |      463 |          470 |               255 |    1.8× |
| sort      |      194 |          187 |                75 |    2.6× |
| strings   |      336 |          321 |               138 |    2.4× |
| tak       |       55 |           16 |                15 |    3.7× |

So, very roughly, a compiled executable runs **2–4× faster than
bytecode, typically around 2.5×**.  Mixed mode only compiles the
program itself, so it matches the executable where the time is in the
program's own code (fib, tak) and matches bytecode where the time is
in the still-bytecode library (format, hash, strings); the executable,
whose runtime is native too, is faster across the board.  These are
microbenchmarks — real programs land wherever their mix of program
code, library code and allocation puts them — and `eval` (262 ms as
bytecode) has no executable column by design.

`make check` runs the native suites when the pieces exist:
`native.test` (mixed mode), `native-parity.test` (every test program,
both ways, byte-identical), `standalone.test` (executables: plain,
eval both ways, stripped, whole-program, lowered), and
`check-native-fixpoint` in `src/world` (the native world's compiler
reproduces every world object byte-identically).

For cross targets, `tests/qemu-native.sh` builds a test program into a
statically linked executable for another architecture and runs it
under qemu-user; see its header for the arguments (a zig compiler from
`pip install ziglang` works as the cross compiler, no system install).

## Debugging aids

- `--keep` (oakc) keeps the generated C and logs.
- `OAK_RT_TRACE=1` logs the control helpers (throw, continuations).
- `--trace-traps` (emulator, non-FAST builds) also logs native traps.
- `#*native-exclusions` (a list of instruction names, or `(all)`)
  makes the translator hand those instructions to the emulator — a
  bisection aid for mixed mode only.
- Error messages and behavior are identical to bytecode: the error
  system is world code, compiled like everything else.
