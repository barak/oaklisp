# Live World Dump

Oaklisp supports dumping the current world image to a file while the
system continues running.  This is in addition to the existing
`--dump` command-line option, which dumps the world only upon exit.

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

## Implementation

The feature spans three layers:

1. **VM instruction 73 (DUMP-WORLD)** in `src/emulator/loop.c` —
   extracts the filename, runs a GC, dumps the world via the existing
   `dump_world()` routine, then resumes execution.

2. **Opcode definition** in `src/world/assembler.oak` —
   `(define-opcode dump-world (0 73) in2 out1 ns)`.

3. **Oaklisp bindings** — `%dump-world` primitive in `src/world/gc.oak`,
   user-facing `dump-world` wrapper in `src/world/dump-world.oak`
   (loaded into the standard world via MISCFILES).

## Notes

- A GC warning about a locative having its raw cell moved may appear
  during the dump.  This is harmless.
- The dump format is the same binary format used by `--dump`, so
  dumped worlds are loaded with `--world` as usual.
- The dumped world includes all state present at dump time, including
  any loaded files and defined variables.
