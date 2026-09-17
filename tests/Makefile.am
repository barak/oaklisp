# This file is part of Oaklisp.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# The GNU GPL is available at http://www.gnu.org/licenses/gpl.html
# or from the Free Software Foundation, 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA

# The test suite: "make check" runs every *.test script here with the
# emulator and world just built.  Each script exits 0 on success, 77
# to skip (e.g. no Guile), and anything else on failure; the details
# are in tests/NAME.log.  The .oak files are the test programs the
# scripts run; they print PASS and FAIL lines which the scripts count.
#
# "make bench" runs the benchmarks in bench.sh and prints a table of
# CPU times; see that script for how to compare two builds.

TEST_EXTENSIONS = .test
AM_TESTS_ENVIRONMENT = \
  OAKLISP='$(abs_top_builddir)/src/emulator/oaklisp'; export OAKLISP; \
  OAKWORLD='$(abs_top_builddir)/src/world/oakworld.bin'; export OAKWORLD; \
  GUILE='$(GUILE)'; export GUILE; \
  top_srcdir='$(abs_top_srcdir)'; export top_srcdir; \
  top_builddir='$(abs_top_builddir)'; export top_builddir; \
  MAKE='$(MAKE)'; export MAKE;

TESTS = smoke.test arith.test bignum.test data.test control.test objects.test \
  io.test locales.test unit.test examples.test compile.test		\
  world.test consistency.test threads.test

OAKTESTS = arith.oak bignum.oak data.oak control.oak objects.oak io.oak locales.oak \
  compile-me.oak

BENCHES = bench-tak.oak bench-fib.oak bench-bignum.oak bench-sort.oak	\
  bench-cons.oak bench-strings.oak bench-hash.oak bench-dispatch.oak	\
  bench-eval.oak bench-format.oak

EXTRA_DIST = $(TESTS) $(OAKTESTS) $(BENCHES) testlib.sh bench.sh

bench: all
	$(AM_TESTS_ENVIRONMENT) $(SHELL) $(srcdir)/bench.sh $(BENCHFLAGS)

.PHONY: bench

CLEANFILES = *.oa *.oa-tmp *.tmp *.bin *.cold *.sym header.txt
clean-local:
	rm -rf compile-me-check
