Oaklisp
=======

Oaklisp is an object-oriented dialect of lisp sharing the standard
lisp syntax, including common lisp style macros, first class types,
multiple inheritance, and multiple namespaces (packages).  Oaklisp is
also a Lisp-1 dialect meaning functions and variables share the same
namespace (like Scheme).

This is a portable implementation of a lisp interpreter / compiler for
the Oaklisp dialect of lisp.

Project homepage(s)

*  https://alioth.debian.org/projects/oaklisp/ (main homepage)
*  https://github.com/barak/oaklisp            (collaborative development)
*  http://www.bcl.hamilton.ie/~barak/oaklisp/  (ancient history)

The compiler compiles Oaklisp source code into byte-code for the
included Oaklisp emulator / virtual machine.  The implmentation
is described in the included documentation, and also in

* Kevin J. Lang and Barak A. Pearlmutter.  Oaklisp: an object-oriented
  Scheme with first class types. In OOPSLA-86, pages 30–7. doi:
  10.1145/960112.28701.  Special issue of ACM SIGPLAN Notices 21(11).
  URL http://barak.pearlmutter.net/papers/oaklisp-oopsla-1986.pdf

* Kevin J. Lang and Barak A. Pearlmutter. Oaklisp: an object-oriented
  dialect of Scheme. Lisp and Symbolic Computation, 1(1):39–51, May
  1988.
  URL http://barak.pearlmutter.net/papers/lasc-oaklisp-1988.pdf

* Barak A. Pearlmutter. Garbage collection with pointers to individual
  cells.  Communications of the ACM, 39(12):202–6, December 1996.
  doi: 10.1145/272682.272712.
  URL http://barak.pearlmutter.net/papers/cacm-oaklisp-gc-1996.pdf

* Barak A. Pearlmutter and Kevin J. Lang. The implementation of
  Oaklisp.  In Peter Lee, editor, Topics in Advanced Language
  Implementation, pages 189–215. MIT Press, 1991.

See BUILD.md for instructions on how to build the system.
