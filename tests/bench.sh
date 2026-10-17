#!/bin/sh
# Benchmarks.  Run with "make bench" (in tests/, or at the top level),
# or directly with OAKLISP and OAKWORLD set.  Each bench-NAME.oak
# defines (bench); it is compiled once, then run REPEAT times, and
# the best CPU time in milliseconds is printed, as measured by
# (get-time) around the call, so that emulator start up and loading
# are not counted.  Use BENCHFLAGS='-r N' for a different number of
# repetitions, and BENCHFLAGS='-o FILE' to also save the results as
# "name ms" lines; two such files can be compared with
#   sh tests/bench.sh -c old.txt new.txt
# BENCHFLAGS=-n compiles the benchmarks with --compile-native instead
# of --compile, for comparing natively compiled code with bytecode.
# BENCHFLAGS=-x builds each benchmark as a stand-alone executable with
# oakc (always best of 5; needs "make -C src/world native-world"),
# BENCHFLAGS=-w does that in whole-program mode and BENCHFLAGS=-i with
# the C-locals lowering, so the modes compare: bytecode, mixed native,
# separate, whole-program and lowered compilation.
#
# The result also depends on the C compiler and its options; compare
# builds made the same way, e.g. with and without --enable-threads.

. "$(dirname "$0")/testlib.sh"

repeat=5
outfile=
compile=--compile
while test $# -gt 0; do
  case $1 in
    -r) repeat=$2; shift 2;;
    -n) compile=--compile-native; shift;;
    -x) compile=exe; shift;;
    -w) compile=exe; oakcflags=-w; shift;;
    -i) compile=exe; oakcflags=--ir; shift;;
    -o) outfile=$2; shift 2;;
    -c) shift; old=$1; new=$2
	printf '%-12s %10s %10s %8s\n' benchmark "$old" "$new" ratio
	while read name ms; do
	  nms=$(awk -v n="$name" '$1==n {print $2}' "$new")
	  test -n "$nms" || continue
	  printf '%-12s %10s %10s %8s\n' "$name" "$ms" "$nms" \
	    "$(awk -v a="$ms" -v b="$nms" 'BEGIN {printf "%.2f", (a>0)? b/a : 0}')"
	done < "$old"
	exit 0;;
    *) echo "usage: bench.sh [-n] [-r repeat] [-o outfile] | -c old new" >&2; exit 2;;
  esac
done

workdir=${TMPDIR:-/tmp}/oaklisp-bench.$$
mkdir -p "$workdir" || exit 1
trap 'rm -rf "$workdir"' 0

echo "Oaklisp benchmarks: best of $repeat runs, CPU milliseconds"
"$OAKLISP" --version
printf '%-12s %8s\n' benchmark ms
test -z "$outfile" || : > "$outfile"

if test "$compile" = exe; then
  OAKC=$top_builddir/oakc
  test -x "$OAKC" || { echo "no oakc at $OAKC"; exit 1; }
  test -f "$top_builddir/src/world/native/liboaknat.a" \
    || { echo "no native runtime (make -C src/world native-world)"; exit 1; }
fi

for f in "$testdir"/bench-*.oak; do
  name=$(basename "$f" .oak); name=${name#bench-}
  cp "$f" "$workdir/$name.oak"
  if test "$compile" = exe; then
    cp "$testdir/benchmain.oak" "$workdir/"
    (cd "$workdir" && "$OAKC" $oakcflags "$name.oak" benchmain.oak -o "$name-x" \
       > "$name.compile.log" 2>&1) \
      || { echo "$name: oakc failed (see $workdir/$name.compile.log)"; continue; }
    ms=$(cd "$workdir" && OAKWORLD=/nonexistent "./$name-x" 2>&1 \
	  | tail -n 1 | sed 's/.*[^0-9]\([0-9][0-9]*\)$/\1/')
  else
  (cd "$workdir" && oak -- $compile "$name" --exit > "$name.compile.log" 2>&1) \
    || { echo "$name: compile failed (see $workdir/$name.compile.log)"; continue; }
  ms=$(cd "$workdir" && oak -- --load "$name" \
	--eval "(let loop ((i 0) (best #f)) (if (= i $repeat) (print best standard-output) (let* ((t0 (get-time)) (v (bench)) (t (- (get-time) t0))) (loop (+ i 1) (if (or (not best) (< t best)) t best)))))" \
	--exit 2>&1 | tail -n 1 | sed 's/.*[^0-9]\([0-9][0-9]*\)$/\1/')
  fi
  case $ms in
    ''|*[!0-9]*) echo "$name: failed"; continue;;
  esac
  printf '%-12s %8s\n' "$name" "$ms"
  test -z "$outfile" || echo "$name $ms" >> "$outfile"
done
