// This file is part of Oaklisp.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// The GNU GPL is available at http://www.gnu.org/licenses/gpl.html
// or from the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA


/**********************************************************************
 *  Copyright (c) by Barak A. Pearlmutter and Kevin J. Lang, 1987-99. *
 *  Copyright (c) by Alex Stuebinger, 1998-99.                        *
 *  Distributed under the GNU General Public License v2 or later      *
 **********************************************************************/

/* This file contains many tunable parameters */

#define _REENTRANT

#include "config.h"
#include "data.h"
#include "stacks.h"

/* spaces */

space_t new_space, old_space, spatic;

ref_t *free_point = 0;

#ifndef THREADS
/* stacks, including default buffer size & fill target */
oakstack value_stack = {1024, 1024/2};
oakstack context_stack = {512, 512/2};
#endif


/* Virtual Machine registers */
#ifdef THREADS

ref_t e_t, e_nil, e_fixnum_type, e_loc_type, e_cons_type,
  e_env_type, *e_subtype_table, e_object_type, e_segment_type, e_boot_code,
  *e_arged_tag_trap_table, *e_argless_tag_trap_table,
  e_uninitialized, e_method_type, e_operation_type = 0;
size_t e_next_newspace_size;
size_t original_newspace_size = 128 * 1024;

#else

ref_t *e_bp, *e_env, e_t, e_nil, e_fixnum_type, e_loc_type, e_cons_type,
  e_env_type, *e_subtype_table, e_object_type, e_segment_type, e_boot_code,
  e_code_segment,
  *e_arged_tag_trap_table, *e_argless_tag_trap_table, e_current_method,
  e_uninitialized, e_method_type, e_operation_type, e_process = 0;
register_set_t *reg_set;
size_t e_next_newspace_size;
size_t original_newspace_size = DEFAULT_NEWSPACE * 1024;
instr_t *e_pc;
unsigned e_nargs = 0;

#endif

/* This should generally be defined in the Makefile */
#ifndef DEFAULT_WORLD
#define DEFAULT_WORLD "/usr/lib/oaklisp/oakworld.bin"
#endif

char *world_file_name = DEFAULT_WORLD;
char *dump_file_name = "oakworld-dump.bin";
int dump_base = 2;		/* 2=binary, other=ascii */
bool dump_flag = false;

int trace_gc = 0;
