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

enum {
  FLAG = 0,
  HELP_ARG,
  WORLD_ARG,
  DUMP_ARG,
  DUMP_BASE_ARG,
  PREDUMP_GC,
  HEAP_ARG,
  VALSIZ_ARG,
  VALHYS_ARG,
  CXTSIZ_ARG,
  CXTHYS_ARG,
  VERBOSE_GC,
};


void 
usage (char *prog)
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
	  /* "\t--audit [file]\n" */
	  "\t--world file         file is world to load\n"
	  "\t--band file          synonym for --world\n"
	  "\t--dump file          dump world to file upon exit\n"
	  "\t--dump-base b        0=ascii, 2=binary; default=2\n"
	  "\t--predump-gc b       0=no, 1=yes; default=1\n"
	  "\n"
	  "\t--size-heap n        n is in kilo-refs\n"
	  "\t--size-val-stk n     n is in refs\n"
	  "\t--size-val-hyst n    n is in refs\n"
	  "\t--size-cxt-stk n     n is in refs\n"
	  "\t--size-cxt-hyst n    n is in refs\n"
	  "\n"
	  "\t--trace-gc v         0=quiet, 3=very detailed; default=0\n"
	  "\t--verbose-gc v       synonym for --trace-gc\n"
	  "\t--trace-traps\n"
#ifndef FAST
	  "\t--trace-segs         trace stack segment writes/reads\n"
#endif
	  "\t--trace-files        trace filesystem operations\n"
	  "\n"
	  "    oaklisp options:\n"
	  "\n"
	  "\tsee users manual, or type (MAP CAR COMMANDLINE-OPTIONS)\n"
	  "\tto a running oaklisp\n"
	  "\n",
	  prog);
}


/* These store the command line arguments not eaten by the emulator,
   which the running world can access. */
int program_argc;
char **program_argv;

int program_arg_char(int arg_index, int char_index)
{
  char *a;
  if (arg_index >= program_argc) return -1;
  a = program_argv[arg_index];
  if (char_index > strlen(a)) return -1;
  return a[char_index];
}



void
parse_cmd_line (int argc, char **argv)
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
      static struct option long_options[] = {
	{"help", no_argument, 0, HELP_ARG},
	{"world", required_argument, 0, WORLD_ARG},
	{"band", required_argument, 0, WORLD_ARG}, /* deprecated */
	{"dump", required_argument, 0, DUMP_ARG},
	{"dump-base", required_argument, 0, DUMP_BASE_ARG},
	{"predump-gc", required_argument, 0, PREDUMP_GC},
	{"size-heap", required_argument, 0, HEAP_ARG},
	{"size-val-stk", required_argument, 0, VALSIZ_ARG},
	{"size-val-hyst", required_argument, 0, VALHYS_ARG},
	{"size-cxt-stk", required_argument, 0, CXTSIZ_ARG},
	{"size-cxt-hyst", required_argument, 0, CXTHYS_ARG},
	{"verbose-gc", required_argument, 0, VERBOSE_GC},
	{"trace-gc", required_argument, 0, VERBOSE_GC},
	{"trace-traps", 0, &trace_traps, true},
#ifndef FAST
	{"trace-segs", 0, &trace_segs, true},
#endif
	{"trace-files", 0, &trace_files, true},
	{0, 0, 0, 0}};

      retval = getopt_long_only(argc, argv,
				"", long_options,
				&option_index);

      if (retval == EOF)
	break;

      switch (retval)
	{
	default:
	  fprintf (stderr, "error: command line syntax\n");
	case '?':
	  /* getopt_long_only() already printed an error message. */
	  usage (argv[0]);
	  exit (EXIT_FAILURE);
	  break;

	case FLAG:
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
	  dump_base = atoi (optarg);
	  if (dump_base != 2 && dump_base != 10 && dump_base != 16)
	    {
	      fprintf (stderr, "Error (command line parser): invalid"
		       " dump base %s.\n", optarg);
	      exit (EXIT_FAILURE);
	    }
	  break;

	case PREDUMP_GC:
	  gc_before_dump = atoi(optarg);
	  break;

	case HEAP_ARG:
	  original_newspace_size = 1024 * atol(optarg);
	  break;

	case VALSIZ_ARG:
	  value_stack_size = atoi(optarg);
	  break;

	case VALHYS_ARG:
	  value_stack_hysteresis = atoi(optarg);
	  break;

	case CXTSIZ_ARG:
	  context_stack_size = atoi(optarg);
	  break;

	case CXTHYS_ARG:
	  context_stack_hysteresis = atoi(optarg);
	  break;

	case VERBOSE_GC:
	  trace_gc = atoi(optarg);
	  break;

	case HELP_ARG:
	  usage (argv[0]);
	  exit (EXIT_SUCCESS);
	  break;
	}
    }

  /* put remainder of command line in variables accessed by Oaklisp-level
     argline instructions */

  program_argc = argc - optind;
  program_argv = argv + optind;

  /* other setup */

#ifdef NOTBAP
#ifndef STACKS_STATIC
  value_stack_size = config.value_stack_size;
  value_stack_hysteresis = config.value_stack_hysteresis;
  context_stack_size = config.context_stack_size;
  context_stack_hysteresis = config.context_stack_hysteresis;
#endif
  original_newspace_size = config.heap_size;
#endif

  return;
}
