/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/


#include "config.h"

#ifndef FAST
const char *ArglessInstrs[] =
{
  "NOOP",			/* 0 */
  "PLUS",
  "NEGATE",
  "EQ?",
  "NOT",
  "TIMES",
  "LOAD-IMM",
  "DIV",
  "=0?",
  "GET-TAG",
  "GET-DATA",			/* 10 */
  "CRUNCH",
  "GETC",
  "PUTC",
  "CONTENTS",
  "SET-CONTENTS",
  "LOAD-TYPE",
  "CONS",
  "<0?",
  "MODULO",
  "ASH",			/* 20 */
  "ROT",
  "STORE-BP-I",
  "LOAD-BP-I",
  "RETURN",
  "ALLOCATE",
  "ASSQ",
  "LOAD-LENGTH",
  "PEEK",
  "POKE",
  "MAKE-CELL",			/* 30 */
  "SUBTRACT",
  "=",
  "<",
  "BIT-NOT",
  "LONG-BRANCH",
  "LONG-BRANCH-NIL",
  "LONG-BRANCH-T",
  "LOCATE-BP-I",
  "LOAD-IMM-CON",
  "CAR",			/* 40 */
  "CDR",
  "SET-CAR",
  "SET-CDR",
  "LOCATE-CAR",
  "LOCATE-CDR",
  "PUSH-CXT-LONG",
  "CALL-PRIMITIVE",
  "THROW",
  "OBJECT-HASH",
  "OBJECT-UNHASH",		/* 50 */
  "GC",
  "BIG-ENDIAN?",
  "VLEN-ALLOCATE",
  "INC-LOC",
  "FILL-CONTINUATION",
  "CONTINUE",
  "REVERSE-CONS",
  "MOST-NEGATIVE-FIXNUM?",
  "FX-PLUS",
  "FX-TIMES",			/* 60 */
  "GET-TIME",
  "REMAINDER",
  "QUOTIENTM",
  "FULL-GC",
  "MAKE-LAMBDA",
};

const char *Instrs[] =
{
  "#<Undefined IVAR 1038>",	/* 0 */
  "HALT",
  "LOG-OP",
  "BLT-STK",
  "BRANCH-NIL",
  "BRANCH-T",
  "BRANCH",
  "POP",
  "SWAP",
  "BLAST",
  "LOAD-IMM-FIX",		/* 10 */
  "STORE-STK",
  "LOAD-BP",
  "STORE-BP",
  "LOAD-ENV",
  "STORE-ENV",
  "LOAD-STK",
  "MAKE-BP-LOC",
  "MAKE-ENV-LOC",
  "STORE-REG",
  "LOAD-REG",			/* 20 */
  "FUNCALL-CXT",
  "FUNCALL-TAIL",
  "STORE-NARGS",
  "CHECK-NARGS",
  "CHECK-NARGS-GTE",
  "STORE-SLOT",
  "LOAD-SLOT",
  "MAKE-CLOSED-ENVIRONMENT",
  "PUSH-CXT",
  "LOCATE-SLOT",		/* 30 */
  "STREAM-PRIMITIVE",
  "FILLTAG",
  "^SUPER-CXT",
  "^SUPER-TAIL",
};
#endif
