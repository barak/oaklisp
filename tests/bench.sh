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
#
# The result also depends on the C compiler and its options; compare
# builds made the same way, e.g. with and without --enable-threads.

. "$(dirname "$0")/testlib.sh"

repeat=5
outfile=
while test $# -gt 0; do
  case $1 in
    -r) repeat=$2; shift 2;;
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
    *) echo "usage: bench.sh [-r repeat] [-o outfile] | -c old new" >&2; exit 2;;
  esac
done

workdir=${TMPDIR:-/tmp}/oaklisp-bench.$$
mkdir -p "$workdir" || exit 1
trap 'rm -rf "$workdir"' 0

echo "Oaklisp benchmarks: best of $repeat runs, CPU milliseconds"
"$OAKLISP" --version
printf '%-12s %8s\n' benchmark ms
test -z "$outfile" || : > "$outfile"

for f in "$testdir"/bench-*.oak; do
  name=$(basename "$f" .oak); name=${name#bench-}
  cp "$f" "$workdir/$name.oak"
  (cd "$workdir" && oak -- --compile "$name" --exit > "$name.compile.log" 2>&1) \
    || { echo "$name: compile failed (see $workdir/$name.compile.log)"; continue; }
  ms=$(cd "$workdir" && oak -- --load "$name" \
	--eval "(let loop ((i 0) (best #f)) (if (= i $repeat) (print best standard-output) (let* ((t0 (get-time)) (v (bench)) (t (- (get-time) t0))) (loop (+ i 1) (if (or (not best) (< t best)) t best)))))" \
	--exit 2>&1 | tail -n 1 | sed 's/.*[^0-9]\([0-9][0-9]*\)$/\1/')
  case $ms in
    ''|*[!0-9]*) echo "$name: failed"; continue;;
  esac
  printf '%-12s %8s\n' "$name" "$ms"
  test -z "$outfile" || echo "$name $ms" >> "$outfile"
done
