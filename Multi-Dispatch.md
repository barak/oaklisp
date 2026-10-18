# Multi-Dispatch for Oaklisp

Oaklisp dispatches on one argument.  A method is found by searching
the type of the first argument and its supertypes for the operation,
with a special case when there are no arguments at all.  Everything
else a program wants to discriminate on — the second argument of a
binary arithmetic operator, the stream a thing is printed to, the
type being coerced to — has to be written out by hand.

This file describes how to dispatch on the types of several
arguments: what the syntax is, what method it picks, how it is
represented, how it is made fast, and in what order the work is to be
done.  `doc/lim` gets the parts of it that survive implementation.

## 1. The problem

The workaround in use today is `fselect-type` (`src/world/macros1.oak`),
a chain of `subtype?` tests on the remaining argument written inside a
method that dispatched on the first.  `<` on fixnums is typical
(`src/world/numbers.oak`):

```lisp
(define-constant <
  (add-method ((make-fancy-open-coded-operation
		(list foldable-mixin) '((<)) 2 1)
	       (fixnum) x y)
    (fselect-type y
      ((fixnum)   (< x y))
      ((bignum)   (not (negative? y)))
      ((rational) (< (* x (denominator y)) (numerator y)))
      ((float) (if (zero? x)
		   (< x (float-sign y))
		   (< (#^float x) y)))
      (otherwise  (error "Illegal second arg (< ~D ~D).~%" x y)))))

```

There are about thirty such sites in `src/world`, concentrated in
`numbers.oak`, `bignum.oak`, `rational.oak` and `complex.oak`, plus a
scattering of `is-a?` chains elsewhere.  Each has the same defects:
the cases for one operation are gathered in one place instead of
living with the types they concern, so a new numeric type cannot be
added without editing existing files; the search is linear and
re-done on every call, with no cache; and the clause order silently
encodes a precedence that nothing checks.

What the language should accept instead is a method per combination:

```lisp
(add-method (< (fixnum) x (bignum) y) (not (negative? y)))
(add-method (< (fixnum) x (rational) y) ...)
```

## 2. How dispatch works now

Five places implement or depend on the dispatch rule, and all five
have to agree.

**The interpreter.**  `loop.c` cases 21 and 22 (`FUNCALL-CXT`,
`FUNCALL-TAIL`).  The operation is on top of the value stack, the
first argument under it, and `e_nargs` holds the count.  Three tiers:

1. the `lambda?` slot of the operation — a method there is a simple
   lambda and runs with no search at all;
2. a one-entry cache in the operation, `[cache-type, cache-method,
   cache-type-offset]`, keyed on the type of the first argument
   (`object` when `e_nargs` is zero);
3. `find_method_type_pair()`, a depth-first, left-to-right walk of
   the supertype DAG consulting each type's `operation-method-alist`,
   with a move-to-front reordering of the alists it searches.

Then `e_bp = REF_TO_PTR(arg1) + lookup_bp_offset(arg1_type, meth_type)`.

**The shared search.**  `src/emulator/dispatch.{c,h}` hold
`find_method_type_pair()` and `lookup_bp_offset()`, factored out of
`loop.c` so a stand-alone executable can link without the
interpreter.

**Natively compiled code.**  `src/emulator/oak-native.h` has
`ON_FUNCALL` and `ON_SITE_CALL`, a per-call-site inline cache that
refills from the operation's own cache; `oak_native_find_method()` in
`dispatch.c` backs it.

**The native compiler.**  `src/world/native-compiler.oak` resolves
calls statically when it can, emitting a guarded direct jump instead
of a dispatch; the guard proves the operation and the first argument's
type, through `%find-method-entry`, `%method-for` and
`%bp-offset-for` (`src/world/load-file.oak`).

**The Guile host.**  `src/cold-compiler/host/kernel.scm` implements
the same search for the Oaklisp that runs under Guile, which hosts the
compiler during a from-source bootstrap.

Two facts about object layout matter throughout.  A type's
`type-bp-alist` maps each of its ancestors (itself included) to the
offset at which *that ancestor's own* instance variables begin within
an instance, and a type's `ivar-list` holds only its own instance
variables, not inherited ones (`src/world/kernel1-maketype.oak`).
That is why `subtype?` is a single `%assq`, and why an `add-method`
may only name instance variables the target type itself declares.
And an operation is five words — `%simple-operation-length` in
`src/world/subprimitive.oak`, `OPERATION_LENGTH` in
`src/emulator/data.h` — handbuilt at cold load in `kernel0.oak`.

## 3. What CLOS did

CLOS is the standard reference for this, and its implementations are
the standard reference for making it fast.

The language defines a *generic function* whose methods carry a
specializer per required parameter.  A call computes the set of
applicable methods, sorts it by *argument precedence order* —
comparing specializers left to right, and at the first argument where
two methods differ, preferring the one whose specializer appears
earlier in that argument's class precedence list — and runs the most
specific, with `call-next-method` chaining to the rest (Bobrow et al.
1988, §1.9–1.10; Steele 1990, ch. 28).  Where the precedence lists of
two applicable methods disagree in opposite directions the call is an
error rather than a silent choice.

PCL, the portable implementation that became the basis of most
others, is where the representation lessons are (Kiczales and
Rodriguez 1990).  Each class has a *wrapper* holding random hash
values.  A generic function has a memoization cache; the key for a
multi-argument dispatch is the **sum** of the wrapper hashes of the
arguments that participate, masked to the cache size.  Argument
positions on which every method specializes to `t` are skipped, so a
function that is effectively single-dispatch pays single-dispatch
costs.  A zero hash value forces a miss, which is how invalidation
works: changing a class zeroes its wrapper's hashes rather than
hunting down the caches that mention it.  A collision tries a second
hash index before the cache is grown.  A miss computes the *effective
method* — a closure chaining the applicable methods, which is what
makes `call-next-method` an ordinary variable reference rather than a
runtime search — and memoizes it against the key.  SBCL's description
of the same machinery, as it stands after thirty years of use, notes
that the extra hash seeds turned out to earn very little and were
removed (SBCL Internals, "The Cacheing Mechanism"; Wingo 2008 measures
the result).

Worth taking: memoize on the tuple of participating argument types;
compute applicability, ordering and the effective method only on a
miss; skip positions nothing specializes on; invalidate lazily.

Worth leaving: the `:before`/`:after`/`:around` method combinations,
`eql` specializers, and — at least at first — hashing.  PCL hashes on
object identity, which needs a stable per-object hash value; Oaklisp's
collector moves objects, so a hash-indexed cache would mean adding a
hash slot to every type, which reaches `data.h`, `kernel0.oak`,
`tool.oak`'s handbuilt `type-size`, `oak-cold-linker.c` and the Guile
host.  Real multimethod sets are small — the numeric tower has a
handful of type pairs per operation — so a short cache compared by
type identity is the thing to build first.  It is also GC-safe without
further thought, since its entries are ordinary refs that the
collector relocates.

The literature on dispatching efficiently on several arguments without
hashing is largely about building a lookup automaton from the method
set (Chambers 1992; Amiel, Gruber and Simon 1994; Chambers and Chen
1999).  That is the right answer for a static whole-program compiler
and the wrong one for a system where `add-method` runs at the REPL; it
is noted here as the direction the native compiler could eventually
take for a closed world, not as the plan.

## 4. Syntax

A method's parameter list becomes a sequence of optionally specialized
parameters:

```
add-method-form ::= (add-method (op-expr param ... [. rest-var]) body ...)
param           ::= [ (type-expr slot-spec ...) ] var
slot-spec       ::= slot | (var slot)
```

A parenthesized group before a parameter gives that parameter's
specializer and opens the named instance variables of that type for
reading and assignment within the body.  A bare symbol is an
unspecialized parameter, which is to say one specialized on `object`.
The rest variable cannot be specialized.

This is not a new form but a generalization of the existing one.
Today's `(add-method (op (type ivar ...) arg ...) body)` is already
an instance of the rule, with the group attached to the first
parameter.  Today's `(add-method (op x y) body)` is the case with no
groups at all, which `macros0.oak`'s expander already turns into a
method on `object` by inserting `('object)`.  And a parenthesized item
in any later parameter position is at present a syntax error — the
contour node requires every variable name to be a symbol
(`src/world/mac-compiler1.oak`) — so nothing that works now changes
meaning.

The `(var slot)` form names a slot while binding it to a different
variable.  It is needed as soon as two parameters have the same type,
since the slot names would otherwise collide:

```lisp
(add-method (plus (complex (xr re) (xi im)) x
		  (complex (yr re) (yi im)) y)
  (make complex (+ xr yr) (+ xi yi)))
```

and it is independently useful in ordinary single-dispatch methods
whenever a slot name is wrong for the body or shadows something.

Examples:

```lisp
(add-method (< (fixnum) x (bignum) y) (not (negative? y)))
(add-method (print-on (object) self (my-stream buffer) s) ...)
(add-method (area (circle r) self) (* 3 r r))    ; unchanged
(add-method (f x y) ...)                         ; unchanged: a lambda
```

## 5. Semantics

**Home type.**  A method is stored, as now, in the
`operation-method-alist` of its *first* parameter's specializer, which
defaults to `object`.  Call that the method's home type.  Dispatch
therefore still begins by walking the type graph of argument one; the
remaining specializers select among the methods found there.

**The first hit still wins.**  Oaklisp has never had CLOS's rule that
every applicable method competes.  It takes the first type in the
search order with a method for the operation, and that method shadows
everything above it.  Multi-dispatch keeps exactly that: the methods
sharing an operation and a home type are gathered into one object, the
search returns it where it would have returned a single method, and
the choice is made among *its* entries alone.  A multimethod on a type
shadows the methods on its supertypes just as a single method always
has, and a method on a subtype shadows the multimethod.

This is the conservative reading, and it is what makes the change
safe: an operation with no multimethods dispatches to exactly the
method it always did, type by type.  It also keeps selection cheap,
since every candidate shares a home type and only the parameters after
the first can discriminate.  The price is that a specialized method on
a supertype is not consulted once a subtype has any method at all —
the price Oaklisp has always paid, now visible in a new place.  What
the `otherwise` clause of an `fselect-type` used to do is done by a
method on the home type that leaves the later parameters
unspecialized: that entry applies to everything and is the least
specific of all, so it is the fallback.

**Applicability.**  An entry applies when each of its specializers is
a supertype of (or equal to) the type of the argument in that
position — one `%assq` in that argument's `type-bp-alist` — and a
position with no argument is matched by `object` alone.

**Specificity.**  Given two applicable entries, compare their
specializers left to right.  At the first position where they differ,
the more specific is the one whose specializer comes earlier in the
*precedence list* of that argument's actual type, where the precedence
list is the depth-first, left-to-right pre-order walk of the supertype
DAG with repeats dropped — precisely the order
`find_method_type_pair()` visits.  The entry that runs is the one more
specific than every other applicable entry.

There is always exactly one.  A type's search order totally orders its
ancestors, and every specializer of an applicable entry is one of
them, so at the first position where two entries differ one of them
always comes first; comparing left to right over a total order is
itself a total order on distinct tuples.  Unlike CLOS, then, Oaklisp
has no ambiguous calls to report — the selector keeps a check and
signals if it ever finds none, but that is a guard, not a case.

**Zero arguments.**  The dedicated zero-argument case goes away as a
special case and survives as an instance of the general rule.  A call
with no arguments has no first argument, so its first specializer is
`object` by the same default that an unspecialized parameter gets, and
dispatch starts at `object`.  The C still has to notice it, because
there is no argument to take the type of, but it is one line reading
the general rule rather than a branch with its own logic, and `e_bp`
is left undefined exactly as it is today — a method whose home type is
`object` has no instance variables to point at.

**Forcibles.**  `fselect-type` tries its clauses, and if none matched
and the value is a promise, forces it and starts the case again, so
that `otherwise` sees the forced value.  Selection does the same: when
the entry that would win matched a promise only because its
specializer there is `object`, the promises among the arguments are
forced and the call dispatched again.  A call that fails outright
reaches `failed-funcall` (`src/world/op-error.oak`) and is reported
there as any other failed call is.

**Arity.**  Oaklisp operations are arity-polymorphic: each method
checks its own argument count.  A parameter position beyond the end of
a short call is absent, and only methods that do not specialize there
apply.

## 6. Representation

**Type.**  Unchanged.  This is the main economy of the design: the
per-type alists, the bp alists, the ivar lists and the cold-world
layout all stay as they are, and `tool.oak` and `oak-cold-linker.c`
need no change at all.

**Method sets.**  Where a type's `operation-method-alist` entry held a
`%method`, it may now hold a `%multi-method`
(`src/world/multi-method.oak`): an object with the number of
specializers after the first that any of its methods has, and a list
of entries, each a tuple of those specializers (padded with `object`),
the method, and an operation of its own.

**The entry's own operation.**  Running a chosen entry has to set the
registers, the instance variable frame and the argument count exactly
as an ordinary call does, and the way to get all of that is to make an
ordinary call.  So each entry carries an operation with just that one
method installed.  A method that uses no instance variable is
installed on `object`, where it goes in the `lambda?` slot and takes
up no room in any alist; one that does must go on its home type, so
that dispatch sets the frame from the first argument as usual.

**The emulator.**  The method search is untouched.  Its callers now
ask `is_method()` (`src/emulator/dispatch.h`) about what it returned
instead of comparing with `nil`: a `%multi-method` is not a method, so
the call traps exactly as a missing method does, through
`%arged-tag-trap-table` entry 21 to `failed-funcall`, and nothing is
cached.  The four callers are `loop.c`'s `FUNCALL` and `^SUPER`,
`oak_native_find_method()` and `native-ops.c`'s `^SUPER`; the native
compiler declines to resolve such a call statically.

**Selection** is Oaklisp, in `multi-method.oak`: find the
`%multi-method` the search would reach, keep the applicable entries,
take the most specific, and call its operation on the original
arguments.

**Caching** is the next step and is not yet built.  The plan is two
more slots in the operation, taking `%simple-operation-length` and
`OPERATION_LENGTH` from 5 to 7: the number of leading parameters any
of the operation's methods specializes on, and a small vector of cache
lines `[type-1, ..., type-n, method, bp-offset]` compared by identity
and replaced round-robin, filled from the world on a miss and flushed
the way the one-entry cache is flushed, by parking a fixnum where a
type belongs.  Operations that never see a multimethod keep today's
three tiers exactly, so there is nothing to pay for not using this.

## 7. Instance variables of arguments after the first

This is the one mechanism the symmetric syntax genuinely adds, and it
turns out to need no new VM register and no change to the code-block
format.

`load-bp` reads `e_bp`, which dispatch computes from argument one
alone, so a slot of argument *k* cannot use it.  But the offset is
computable from pieces that already exist.  Because a type's
`type-bp-alist` records where each ancestor's own instance variables
begin, and a type's `ivar-list` holds only its own, the absolute word
offset of a slot of argument *k* is

```
(cdr (%assq specializer_k (type-bp-alist (get-type arg_k)))) + index
```

where `index` is the slot's position in `specializer_k`'s own
`ivar-list` — exactly the arithmetic the VM does for argument one when
it sets `e_bp`.

The three pieces:

- *The index* is resolved at compile time.  `ivar-heuristic`
  (`src/world/mac-compiler3.oak`) already evaluates a method's type
  expression when it can, and nearly always can, since the specializer
  is a global holding a type.  For parameters after the first, require
  that it succeed and signal a clear error when it does not, rather
  than inventing a map: argument one can fall back on install-time
  patching because its map travels in the code block header, and
  widening that header would reach into `remap-your-ivars`,
  `tool.oak` and `oak-cold-linker.c` for very little.
- *The base* is computed once per call, at method entry, into a stack
  local: one `%assq` down the argument's bp alist, in Oaklisp, per
  specialized parameter whose slots the body actually uses.  Methods
  that specialize without naming slots pay nothing.  If this ever
  shows up in a profile, `lookup_bp_offset()` already exists in C and
  can be exposed as a subprimitive.
- *The access* uses existing opcodes.  `%increment-locative` takes a
  runtime offset, so a read is the contents of the argument's locative
  incremented by base plus index, and a write is that locative's
  setter — which gives `(set! yr ...)` on another argument's slot for
  free, as with argument one.

Install-time safety without a format change: the resolved
`(specializer slot index)` triples are passed to
`%install-method-with-env` as an ordinary argument and verified there,
so a type redefined between compiling and loading is caught instead of
silently reading the wrong word.

## 8. Changes by file

- `src/world/macros0.oak`, `src/world/scheme-macros.oak` — the
  `add-method` expanders learn the grammar.  There are two of them,
  since `scheme-locale` gives dotted parameter lists their R3RS
  meaning; they should share a parser.
- `src/world/mac-compiler1.oak`, `mac-compiler-nodes.oak`,
  `mac-compiler3.oak`, `mac-compiler2.oak`, `mac-code.oak` — the
  method node carries per-parameter specializers and local/slot pairs
  instead of a flat ivar list; `ivar-heuristic` and `munch-together`
  read the slot side, the contour binds the local side.
- `src/world/kernel1-install.oak`, `src/world/code-vector.oak` — both
  `%install-method-with-env` definitions take the specializer tuple
  and the verified triples, and build or extend a `%multi-method`.
- `src/world/kernel0.oak`, `src/world/subprimitive.oak` — the
  operation grows two slots.
- `src/emulator/data.h`, `dispatch.c`, `dispatch.h`, `loop.c` — the
  tuple cache and the multimethod branch of the search.
- `src/emulator/oak-native.h`, `native-ops.c` — the site cache keys on
  a tuple, or declines to cache multi operations at first.
- `src/world/native-compiler.oak`, `src/world/load-file.oak` — static
  resolution either accounts for multimethods or refuses to resolve
  them; refusing is correct and is the first step.
- `src/cold-compiler/host/kernel.scm` — the same dispatch rule, which
  must land before any world source uses a multimethod.
- `src/world/has-method.oak`, `anonymous.oak`, `op-error.oak`, and the
  `describe`/`trace`/`apropos` paths — everything that walks an
  `operation-method-alist` must expect a `%multi-method` in it.
- `doc/lim/methods.tex`, `doc/lang`, `doc/summary` — documentation.

## 9. Order of work

Each step builds, bootstraps, and passes `make check` by itself.

0. **Oracle and baseline.**  A test that walks every operation and
   every type in the built world and checks that the new specificity
   rule picks the method the current search picks.  This is cheap, and
   it is the decisive guard against disturbing existing code.  Plus
   the benchmark table in §10.
1. **`(var slot)` aliasing**, on argument one only.  Splits the method
   node's ivar list into local and slot names.  Useful on its own, and
   it lands the parser and compiler surgery before any dispatch change
   rides on it.  *Done.*
2. **The general grammar and a correct slow path.**  Specializers in
   every position, `%multi-method` in the alists, applicability,
   specificity and ambiguity in Oaklisp, reached by having the
   emulator trap when the alist yields a non-method.  Multimethods
   work; they are slow.  *Done.*  The Guile host needed no change of
   its own: it hosts the world's compiler rather than reimplementing
   it, and nothing in the world uses a multimethod yet.
3. **The tuple cache** in C, filled by the world on a miss.  The
   single-dispatch path must be unchanged within measurement noise.
4. **Slots of arguments after the first**, per §7.  Separable from
   dispatch, and testable with an ordinary single-dispatch method that
   happens to name a second argument's slots.
5. **`call-next-method`**, if wanted, as effective-method closures
   carrying the remaining applicable methods in the environment
   vector, memoized per tuple.  This is how CLOS does it and it needs
   no VM change; the 3-word context frame stays 3 words.
6. **Convert the world**: the comparison and arithmetic operators in
   `numbers.oak`, `bignum.oak`, `rational.oak`, `complex.oak`.  A
   source file using a multimethod cannot be compiled by a bootstrap
   world that lacks the installer, so this waits a release or uses the
   `OAKEXTRALOAD` preload already in `src/world/Makefile.am`.  Cold
   files stay single-dispatch.
7. **Documentation and tests.**

## 10. Benchmarks

Measured before any of this is built, so the native implementation has
something to beat.  `tests/bench-multi.oak` times the idioms that
multi-dispatch replaces; `make bench` runs it as one number, and

```sh
oaklisp -- --load tests/bench-multi --eval '(breakdown)' --exit
```

prints the breakdown.  Four types under a common supertype, one
binary operation per idiom, all computing the same thing from the
types of both arguments over the same sixteen argument pairs; 640,000
calls per idiom, best of five, CPU milliseconds, on an Intel i7-14700
with gcc 16.2 and the default `--enable-ndebug` build.  Run-to-run
variation is around ten percent.

| idiom | ms | what it is |
|-------|-----|------------|
| `one/mono`  |  64 | single dispatch, first argument's type fixed: the method cache hits |
| `one/poly`  |  66 | single dispatch, first argument's type varying: the cache misses, full search |
| `fselect`   | 200 | dispatch on the first argument, `fselect-type` on the second |
| `select`    | 179 | the same with `select-type`, which skips the forcible check |
| `is-a?`     | 231 | dispatch on the first argument, a chain of `is-a?` on the second |
| `deep/mono` |  58 | method four levels up a chain, type fixed: the cache hits |
| `deep/poly` |  74 | the same with the type varying: the cache misses and the chain is walked |
| `double`    |  94 | double dispatch: a second operation, chosen per first-argument type |
| `nullary`   |  54 | a zero-argument call, for the path §5 folds into the general rule |

Compare a row only against its own pair: the method bodies differ
between the groups, so `deep` and `one` are not comparable to each
other.

Three things to take from this.  Dispatch itself is cheap — the
difference between a method cache hit and a full search is 3% on a
shallow hierarchy and 28% on a four-level chain, so
`find_method_type_pair()` with its move-to-front alists is nearly as
fast as the cache in front of it.  A multimethod cache has that to
beat, not some slow path.  Second, the manual idioms cost three to four
times a plain dispatch: `fselect-type` more than triples the call, and
the `is-a?` chain is worse still, so there is real room.  Third, the
cheapest workaround available today is plain double dispatch at 94 ms
— two full dispatches and a second operation per first-argument type
still beat one dispatch plus four `subtype?` tests.  A native
multimethod that cannot beat 94 ms is not worth having.

## References

Amiel, Eric, Olivier Gruber, and Eric Simon. "Optimizing Multi-Methods
Dispatch Using Compressed Dispatch Tables." *OOPSLA '94*, 244–258.

Bobrow, Daniel G., Linda G. DeMichiel, Richard P. Gabriel, Sonya E.
Keene, Gregor Kiczales, and David A. Moon. "Common Lisp Object System
Specification." X3J13 Document 88-002R, June 1988; also *ACM SIGPLAN
Notices* 23(SI), September 1988.

Chambers, Craig. "Object-Oriented Multi-Methods in Cecil." *ECOOP '92*,
LNCS 615, 33–56.

Chambers, Craig, and Weimin Chen. "Efficient Multiple and Predicate
Dispatching." *OOPSLA '99*, 238–255.

Deutsch, L. Peter, and Allan M. Schiffman. "Efficient Implementation
of the Smalltalk-80 System." *POPL '84*, 297–302.

Dussud, Patrick H. "TICLOS: An Implementation of CLOS for the Explorer
Family." *OOPSLA '89*, 215–220.

Hölzle, Urs, Craig Chambers, and David Ungar. "Optimizing
Dynamically-Typed Object-Oriented Languages With Polymorphic Inline
Caches." *ECOOP '91*, LNCS 512, 21–38.

Kiczales, Gregor, and Luis Rodriguez. "Efficient Method Dispatch in
PCL." *Proceedings of the 1990 ACM Conference on LISP and Functional
Programming*, 99–105.
<https://groups.csail.mit.edu/mac/users/lhr/misc/papers-misc/PCL-abstract.html>

Kiczales, Gregor, Jim des Rivières, and Daniel G. Bobrow. *The Art of
the Metaobject Protocol*. MIT Press, 1991.

Lang, Kevin J., and Barak A. Pearlmutter. "Oaklisp: An Object-Oriented
Scheme with First Class Types." *OOPSLA '86*, 30–37.

Lang, Kevin J., and Barak A. Pearlmutter. "Oaklisp: An Object-Oriented
Dialect of Scheme." *Lisp and Symbolic Computation* 1(1):39–53, 1988.

SBCL Internals. "The Cacheing Mechanism."
<https://sbcl.org/sbcl-internals/The-Cacheing-Mechanism.html>

Steele, Guy L., Jr. *Common Lisp: The Language*, 2nd ed. Digital
Press, 1990, chapter 28.

Wingo, Andy. "dynamic dispatch: a followup." 19 October 2008.
<https://wingolog.org/archives/2008/10/19/dynamic-dispatch-a-followup>
