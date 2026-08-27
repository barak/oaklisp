Bootstrapping Oaklisp from source
=================================

`oak-bootstrap.scm` compiles Oaklisp `.oak` sources to `.oa` objects
without a running Oaklisp.  It is what `./configure --enable-bootstrap`
uses, and what `make check-bootstrap` in `src/world` runs.

    guile3.0 --no-auto-compile oak-bootstrap.scm --srcdir ../world --outdir OUT \
        --noisy 0 cold-booting kernel0 ... --locale compiler-locale crunch ...

Why it is not a second compiler
-------------------------------

What the Oaklisp compiler emits depends on the world it runs in: a
frozen global (`define-constant`) is inlined as a constant, an open
coded operation like `car` or `(%slot 3)` becomes an instruction, the
layout of a method's instance variables comes from the real type, and
foldable operations are applied at compile time.  A compiler written
from scratch could only be checked by booting its output and seeing
what breaks.  Instead, this hosts enough of Oaklisp in Guile to load
the world's own macros and compiler from `src/world/*.oak` and run
them.  The objects it writes are then byte for byte those a native
Oaklisp writes, and `make check-bootstrap` holds it to that.

What is native and what is loaded
---------------------------------

`host/world.scm` is the plan.  In order:

1. `kernel.scm`, `runtime.scm`, `runtime2.scm` make the types the
   world builder would (`type`, `object`, `operation`, `cons-pair`,
   `string`, ...) with the emulator's exact slot layout
   (kernel1-maketype.oak's algorithm), method dispatch in the
   emulator's order, locatives, fluids, `%catch`/`%throw`, and the
   parts of the world that are written against the VM: strings,
   vectors, hash tables, streams, `format`, `print`, the reader.
2. `multi-off.oak`, `kernel1-freeze.oak` and `fluid.oak`, then the
   macro files `macros0.oak` ... `define.oak` and `backquote.oak`, are
   evaluated with the bootstrap macros in `macros.scm` (host versions
   of `define`, `let`, `cond`, `destructure*`, ... used only until the
   real ones are loaded; the expander looks in the locale first).
3. The cold files are loaded in COLDFILESD order, skipping the ones
   provided natively, with a few hooks (`kernel.oak` re-initializes
   the builder's types, which empties their method tables, so the
   native methods are put back).  When `make-locales.oak` has made
   `system-locale`, the proto locale's variables and macros move
   there.
4. The rest of MISCFILES, for the constants and types they define,
   then the compiler files into `compiler-locale`.

`translate.scm` is the evaluator: `expand-groveling` (expand.oak) and
a translation of the core forms (`%if`, `%block`, `%add-method`,
`%labels`, `%make-locative`, `%catch`) into Guile code that
`primitive-eval` runs.  Lambdas become Guile closures; methods take
the instance's base pointer as their first argument, as the emulator
does implicitly.

Operations that must stay native even when the world redefines them
(`car`, `+`, `contents`, `get-type`, ...) are `native-locked!`: a
hook by global name re-locks whatever the world assigns.  Operations
that exist only to be open coded, such as `(%slot 3)`, have no
methods; `oak-call` interprets their byte code list (`load-slot`,
`store-slot`, `load-reg`, ...) when the host calls them.

Checking it
-----------

    cd src/world && make check-bootstrap

compiles every shipped source into `bootstrap-check/` and compares
with `prebuilt/src/world/`.  To poke at the hosted world:

    guile3.0 --no-auto-compile oak-bootstrap.scm --srcdir ../world \
        --eval '(cc (quote (define (f x) (car x))))'

`--verbose` names each file as it loads, `--keep-going` reports load
errors and carries on, `--trace` prints every form and its
translation.
