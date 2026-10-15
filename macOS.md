Building and installing Oaklisp on macOS
========================================

These steps work on both Apple silicon (arm64) and Intel Macs.  For
the general build, see BUILD.md; this file covers what is different
on a Mac.


Prerequisites
=============

1. Xcode Command Line Tools.  These provide the C compiler (clang,
   which is also installed as gcc), make, and git:

	xcode-select --install

2. Homebrew (https://brew.sh), then the build tools:

	brew install autoconf automake autoconf-archive

   autoconf-archive supplies the AX_* macros that configure.ac uses;
   they are not kept in the repository.  Without them autoreconf
   still succeeds, but configure then prints "AX_CFLAGS_WARN_ALL:
   command not found" and builds without -Wall.

3. Something to bootstrap the world from (see "Bootstrapping" in
   BUILD.md).  A checkout without prebuilt/ needs Guile 3:

	brew install guile

   If you have the prebuilt/ directory from the "prebuilt" branch,
   or an installed Oaklisp world, you don't need Guile.

Optional:

- GNU readline for line editing at the REPL (see Line Editing below):

	brew install readline

- MacTeX, to build the PDF documentation:

	brew install --cask mactex-no-gui

  Without it, configure warns that there is neither latexmk nor
  prebuilt documentation, and skips building PDF documentation.


Quick Start
===========

	autoreconf --install
	./configure
	make
	make check            # optional: runs the test suite
	sudo make install

This installs `oaklisp` in `/usr/local/bin` and the world in
`/usr/local/lib/oaklisp/oakworld.bin`.  To install somewhere that
doesn't need sudo, give configure a prefix:

	./configure --prefix=$HOME/.local
	make
	make install

and add `$HOME/.local/bin` to your PATH.

`make bootstrap` (instead of plain `make`) also rebuilds the world
with itself twice and checks that the compiler reaches a fixpoint, so
the installed world is fully self-hosted.


Line Editing
============

Configure's readline check finds whatever `-lreadline` is.  On macOS,
unless told otherwise, that is the system's libedit: Apple's
`readline/readline.h` is a symlink to `editline/readline.h`.  libedit
works for line editing and history, so the defaults are fine.

To insist on GNU readline from Homebrew instead (it is keg-only, so
configure has to be told where it is):

	./configure --with-readline=yes \
	            CPPFLAGS="-I$(brew --prefix readline)/include" \
	            LDFLAGS="-L$(brew --prefix readline)/lib"

--with-readline=yes makes a missing library an error rather than a
quietly less capable build.

To build without line editing:

	./configure --without-readline

At run time, `oaklisp --no-line-editing` turns line editing off.


Running
=======

	oaklisp                                   # REPL
	oaklisp -- --eval '(print (+ 1 2) standard-output)' --exit
	./src/emulator/oaklisp --world src/world/oakworld.bin   # uninstalled

`--eval` does not print the calculated value, hence the explicit print.

See "Running Oaklisp" in BUILD.md for more.


Notes
=====

- 64-bit only.  macOS cannot build or run 32-bit executables, so
  --disable-64-bit does not work.  Bytecode compiled with
  `--target bc2-32` still loads on a 64-bit Mac.

- Both Apple silicon and Intel Macs are little-endian, so the
  architecture is bc4-el64 by default (bc2-el64 with
  --with-instructions-per-ref=2).

- Big-endian cross builds use qemu-user, which only runs on Linux,
  so they can't be done on a Mac.


Troubleshooting
===============

"use of undeclared identifier 'rl_catch_signals'"
    Your configure was generated before libedit was handled.  Rerun
    autoreconf --install and ./configure.

"no way to build the world"
    Make found neither prebuilt/ nor Guile.  Install Guile
    (brew install guile) or check out prebuilt/ from the "prebuilt"
    branch.  If Guile is installed but not on your PATH, point
    configure at it with --with-guile=$(brew --prefix guile)/bin/guile.

"Permission denied" during make install
    The default prefix /usr/local needs sudo.  Use sudo make install,
    or configure with --prefix=$HOME/.local as shown above.

clangd/editor errors such as "unknown type name 'bool'"
    Your editor is reading the emulator sources without the build's
    flags.  The real build doesn't produce these errors.  To give
    clangd the flags, generate compile_commands.json with
    `bear -- make` (brew install bear).
