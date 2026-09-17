# Live World Dump and Load

Oaklisp supports dumping the current world image to a file while the
system continues running, and loading a world image at runtime to
replace the current one.  These are in addition to the existing
`--dump` and `--world` command-line options.

## Usage

The `dump-world` function is part of the standard world image and
available immediately:

```scheme
(dump-world "snapshot.bin")
;; System continues running after the dump
```

Multiple dumps can be taken at different points to capture evolving state:

```scheme
(define x 42)
(dump-world "snapshot1.bin")

(set! x 99)
(dump-world "snapshot2.bin")
```

Each dump captures the world state at the time it was called.
The dumped files can be loaded normally:

```
oaklisp --world snapshot1.bin   # x is 42
oaklisp --world snapshot2.bin   # x is 99
```

## From the command line

```sh
oaklisp --world oakworld.bin -- \
  --eval '(dump-world "snapshot.bin")' \
  --eval '(format #t "Still running~%")' \
  --exit
```

## API

### `(dump-world filename)` — operation on `string`

Dumps the current world image to *filename* in binary format and
returns `#t`.  The system continues running normally after the dump.
A garbage collection is performed before dumping to reduce the image
size.

### `(%dump-world locative length)` — low-level primitive

Takes a locative to string data and a fixnum length.  This is the
open-coded VM instruction used internally by `dump-world`.  Most users
should call `dump-world` instead.

---

## Loading a World at Runtime

### `(load-world filename)` — operation on `string`

Loads the world image from *filename*, completely replacing the
current world in memory.  **This call never returns.**  The VM
restarts with the loaded world's boot code, which re-processes the
original command-line arguments.

```scheme
(load-world "snapshot.bin")
;; execution never reaches here
```

This is equivalent to restarting Oaklisp with `--world snapshot.bin`,
except it happens within the running process and reuses the same
command-line arguments.

### `(%load-world locative length)` — low-level primitive

Takes a locative to string data and a fixnum length.  This is the
open-coded VM instruction used internally by `load-world`.  Most users
should call `load-world` instead.

### Example: checkpoint and restore

```scheme
;; Build up some state and checkpoint it
(define counter 0)
(set! counter 100)
(dump-world "checkpoint.bin")

;; Later, in another session or after further changes:
(load-world "checkpoint.bin")
;; VM restarts with counter = 100
```

---

## Implementation

### dump-world

Three layers:

1. **VM instruction 73 (DUMP-WORLD)** in `src/emulator/loop.c` —
   extracts the filename, runs a GC, dumps the world via the existing
   `dump_world()` routine, then resumes execution.

2. **Opcode definition** in `src/world/assembler.oak` —
   `(define-opcode dump-world (0 73) in2 out1 ns)`.

3. **Oaklisp bindings** — `%dump-world` primitive in `src/world/gc.oak`,
   user-facing `dump-world` wrapper in `src/world/dump-world.oak`
   (loaded into the standard world via MISCFILES).

### load-world

Three layers:

1. **VM instruction 74 (LOAD-WORLD)** in `src/emulator/loop.c` —
   extracts the filename, frees old heap spaces, calls `read_world()`
   to load the new world, allocates fresh working space, resets the
   stacks, sets boot registers, and restarts the interpreter loop.

2. **Opcode definition** in `src/world/assembler.oak` —
   `(define-opcode load-world (0 74) in2 out0 ns)`.

3. **Oaklisp bindings** — `%load-world` primitive in `src/world/gc.oak`,
   user-facing `load-world` wrapper in `src/world/dump-world.oak`
   (loaded into the standard world via MISCFILES).

## Notes

- A GC warning about a locative having its raw cell moved may appear
  during a dump.  This is harmless.
- The dump format is the same binary format used by `--dump`, so
  dumped worlds are loaded with `--world` or `load-world` as usual.
- The dumped world includes all state present at dump time, including
  any loaded files and defined variables.
- After `load-world`, the loaded world re-processes the original
  command-line arguments.  If those arguments include a call to
  `load-world`, the result is an infinite loop — use `load-world`
  from the REPL or guard it with a condition.
