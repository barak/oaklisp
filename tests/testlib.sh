# Shared shell for the test scripts.  Sourced, not run.
#
# Environment (set by AM_TESTS_ENVIRONMENT in Makefile.am):
#   OAKLISP     the emulator to test
#   OAKWORLD    the world to load
#   top_srcdir, top_builddir, GUILE, MAKE
#
# For running a test by hand: sh tests/arith.test with those set, or
# just rely on the defaults below, which assume an in-tree build.

: ${top_srcdir:=$(cd "$(dirname "$0")/.." && pwd)}
: ${top_builddir:=$top_srcdir}
: ${OAKLISP:=$top_builddir/src/emulator/oaklisp}
: ${OAKWORLD:=$top_builddir/src/world/oakworld.bin}
: ${GUILE:=no}
: ${MAKE:=make}
testdir=$(cd "$(dirname "$0")" && pwd)

# Run Oaklisp non-interactively: oak [emulator options --] oaklisp options
oak() { "$OAKLISP" --batch --world "$OAKWORLD" "$@"; }

# Run a test program and count its PASS and FAIL lines.  Any error
# report from Oaklisp counts as a failure too.  Extra arguments go
# before the --load, e.g. --locale user-locale.
run_oak_test() {
  prog=$1; shift
  out=$(oak -- "$@" --load "$testdir/$prog" --exit 2>&1)
  status=$?
  echo "$out"
  passes=$(printf '%s\n' "$out" | grep -c '^PASS')
  fails=$(printf '%s\n' "$out" | grep -c '^FAIL')
  errors=$(printf "%s\n" "$out" | grep -c "An error occurred\|^Error:\|^\*\*\* ")
  echo "== $prog: $passes passed, $fails failed, $errors errors, exit status $status"
  test "$fails" = 0 && test "$errors" = 0 && test "$status" = 0 && test "$passes" != 0
}

skip() { echo "SKIP: $*"; exit 77; }
fail() { echo "FAIL: $*"; exit 1; }
