/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

/* This file contains many tunable parameters */

#include "config.h"
#include "data.h"

/* Version and greeting */
const char *version = "1.00", *compilation_date = __DATE__, *compilation_time = __TIME__;

/* byte gender */
int byte_gender = little_endian;


/* spaces */

space_t new_space, old_space, spatic;
ref_t *free_point = 0;


/* stacks */

stack_t value_stack = {0, 0, 0, 0};
stack_t context_stack = {0, 0, 0, 0};


#ifndef STACKS_STATIC
int value_stack_size = 4 * 1024;
int value_stack_hysteresis = 10;
int context_stack_size = 4 * 1024;
int context_stack_hysteresis = 10;
#else
ref_t VAL_STACK[value_stack_size + 2];
ref_t CON_STACK[context_stack_size + 2];
#endif


/* Weak pointer table and weak pointer hashtable */

const int wp_table_size = 3000;
const int wp_hashtable_size = 3017;


/* Virtual Machine registers */

register_set_t *reg_set;
#ifdef THREADS
ref_t *e_env, e_t, e_nil, e_fixnum_type, e_loc_type, e_cons_type,
  e_env_type, *e_subtype_table, e_object_type, e_segment_type, e_boot_code,
  *e_arged_tag_trap_table, *e_argless_tag_trap_table,
  e_uninitialized, e_method_type, e_operation_type;

size_t e_next_newspace_size;
size_t original_newspace_size = 128 * 1024;

#else
ref_t *e_bp, *e_env, e_t, e_nil, e_fixnum_type, e_loc_type, e_cons_type,
  e_env_type, *e_subtype_table, e_object_type, e_segment_type, e_boot_code,
  e_code_segment, *e_arged_tag_trap_table, *e_argless_tag_trap_table,
  e_current_method, e_uninitialized, e_method_type, e_operation_type;

size_t e_next_newspace_size;
size_t original_newspace_size = 128 * 1024;

u_int16_t *e_pc;
unsigned e_nargs = 0;
#endif

#ifndef DEFAULT_WORLD
#define DEFAULT_WORLD "/home/bap/usr/oaklisp/src/lib/oakworld.bin"
#endif

char *world_file_name = DEFAULT_WORLD;
char *dump_file_name = "oakworld-dump.bin";
int dump_base = 2;		/* 2=binary, other=ascii */
bool dump_flag = false;

int trace_gc = 0;


