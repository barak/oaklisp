/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "config.h"
#include "data.h"
#include "cmdline.h"
#include "xmalloc.h"
#include "stacks.h"

enum
  {
    FLAG_ARG = 0,
    HELP_ARG,
    WORLD_ARG,
    DUMP_ARG,
    DUMP_BASE_ARG,
    PREDUMP_GC_ARG,
    HEAP_ARG,
    VALSIZ_ARG,
    CXTSIZ_ARG,
    MAX_SEG_ARG,
    VERBOSE_GC_ARG,
  };


void
usage(char *prog)
{
  fprintf(stdout,
	  "The Oaklisp bytecode emulator.\n"
	  "\n"
	  "Usage: %s emulator-options -- oaklisp-options\n"
	  "\n"
	  "    emulator options:\n"
	  "\n"
	  "\t--help               print this message and terminate\n"
	  "\n"
	  "\t--world file         file is world to load\n"
	  "\t--dump file          dump world to file upon exit\n"
	  "\t--d file             synonym for --dump\n"
	  "\t--dump-base b        0=ascii, 2=binary; default=2\n"
	  "\t--predump-gc b       0=no, 1=yes; default=1\n"
	  "\n"
	  "\t--size-heap n        n is in kilo-refs, default %d\n"
	  "\t--size-val-stk n     value stack buffer, n is in refs\n"
	  "\t--size-cxt-stk n     context stack buffer, n is in refs\n"
	  "\t--size-seg-max n     maximum flushed segment len, n is in refs\n"
	  "\n"
	  "\t--trace-gc v         0=quiet, 3=very detailed; default=0\n"
	  "\t--verbose-gc v       synonym for --trace-gc\n"
	  "\t--trace-traps\n"
#ifndef FAST
	  "\t--trace-segs         trace stack segment writes/reads\n"
	  "\t--trace-valcon       print entire value stack at each instr\n"
	  "\t--trace-cxtcon       print entire context stack at each instr\n"
	  "\t--trace-stks         print the size of the stacks at each instr\n"
	  "\t--trace-instructions trace each bytecode executed\n"
	  "\t--trace-methods      trace each method lookup\n"
#ifdef OP_TYPE_METH_CACHE
	  "\t--trace-mcache       trace method cache\n"
#endif
#endif
	  "\t--trace-files        trace filesystem operations\n"
	  "\n"
	  "    oaklisp options:\n"
	  "\n"
	  "\tTry \"man oaklisp\" or run \"%s -- --help\"\n"
	  "\n",

	  /* "\type (MAP CAR COMMANDLINE-OPTIONS) to a running oaklisp\n" */

	  prog, DEFAULT_NEWSPACE, prog);
}


/* These store the command line arguments not eaten by the emulator,
   which the running world can access. */
int program_argc;
char **program_argv;

int
program_arg_char(int arg_index, int char_index)
{
  char *a;
  if (arg_index >= program_argc)
    return -1;
  a = program_argv[arg_index];
  if (char_index > strlen(a))
    return -1;
  return a[char_index];
}



void
parse_cmd_line(int argc, char **argv)
{
  int retval, option_index = 0;

  /* check for correct endianity settings */
  /* main() routine sets byte_gender */
  if (BYTE_GENDER != byte_gender)
    {
      fprintf(stderr, "error: configured for incorrect endianity.\n");
      exit(EXIT_FAILURE);
    }

  /* parse command line arguments */
  while (1)
    {
      static struct option long_options[] =
      {
	{"help", no_argument, 0, HELP_ARG},
	{"world", required_argument, 0, WORLD_ARG},
	{"dump", required_argument, 0, DUMP_ARG},
	{"d", required_argument, 0, DUMP_ARG},
	{"dump-base", required_argument, 0, DUMP_BASE_ARG},
	{"predump-gc", required_argument, 0, PREDUMP_GC_ARG},
	{"size-heap", required_argument, 0, HEAP_ARG},
	{"size-val-stk", required_argument, 0, VALSIZ_ARG},
	{"size-cxt-stk", required_argument, 0, CXTSIZ_ARG},
	{"size-seg-max", required_argument, 0, MAX_SEG_ARG},
	{"trace-gc", required_argument, 0, VERBOSE_GC_ARG},
	{"trace-traps", no_argument, &trace_traps, true},
#ifndef FAST
	{"trace-segs", no_argument, &trace_segs, true},
	{"trace-valcon", no_argument, &trace_valcon, true},
	{"trace-cxtcon", no_argument, &trace_cxtcon, true},
	{"trace-stks", no_argument, &trace_stks, true},
	{"trace-instructions", no_argument, &trace_insts, true},
	{"trace-methods", no_argument, &trace_meth, true},
#ifdef OP_TYPE_METH_CACHE
	{"trace-mcache", no_argument, &trace_mcache, true},
#endif
#endif
	{"trace-files", no_argument, &trace_files, true},
	{0, 0, 0, 0}};

      retval = getopt_long_only(argc, argv,
				"", long_options,
				&option_index);

      if (retval == EOF)
	break;

      switch (retval)
	{
	default:
	  fprintf(stderr, "error: command line syntax\n");
	case '?':
	  /* getopt_long_only() already printed an error message. */
	  usage(argv[0]);
	  exit(EXIT_FAILURE);
	  break;

	case FLAG_ARG:
	  /* variable set by getopt() itself */
	  break;

	case WORLD_ARG:
	  world_file_name = optarg;
	  break;

	case DUMP_ARG:
	  dump_file_name = optarg;
	  dump_flag = true;
	  break;

	case DUMP_BASE_ARG:
	  dump_flag = true;
	  dump_base = atoi(optarg);
	  if (dump_base != 2 && dump_base != 10 && dump_base != 16)
	    {
	      fprintf(stderr, "Error (command line parser): invalid"
		      " dump base %s.\n", optarg);
	      exit(EXIT_FAILURE);
	    }
	  break;

	case PREDUMP_GC_ARG:
	  gc_before_dump = atoi(optarg);
	  break;

	case HEAP_ARG:
	  original_newspace_size = 1024 * atol(optarg);
	  break;

	case VALSIZ_ARG:
	  value_stack.size = atoi(optarg);
	  value_stack.filltarget = value_stack.size/2;
	  break;

	case CXTSIZ_ARG:
	  context_stack.size = atoi(optarg);
	  context_stack.filltarget = context_stack.size/2;
	  break;

	case MAX_SEG_ARG:
	  max_segment_size = atoi(optarg);
	  break;

	case VERBOSE_GC_ARG:
	  trace_gc = atoi(optarg);
	  break;

	case HELP_ARG:
	  usage(argv[0]);
	  exit(EXIT_SUCCESS);
	  break;
	}
    }

  /* Check to make sure that the stacks will work.

     We need the following guarantee: we must be able to pull in
     segments to allow a (LOAD-STK 255) instruction.  This means that
     the stack buffer must be at least 255.

     Furthermore, we must be able to satisfy this by unflushing
     segments.  The unflushing routine only pulls in integral
     segments, so we must be able to unflush a maximal segment if
     there are only 254 elements in the buffer.

     Therefore we must have:

     value_stack.size >= 254 + max_segment_size
  */

  if (value_stack.size < 254 + max_segment_size) {
    value_stack.size = 254 + max_segment_size;
    fprintf(stderr, "warning: using value stack of size %d.\n",
	    value_stack.size);
  }


  /* put remainder of command line in variables accessed by Oaklisp-level
     argline instructions */

  program_argc = argc - optind;
  program_argv = argv + optind;

  return;
}
