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


/* Natively compiled code: loading modules, finding a stub code
   vector's C function, and the slow paths of the generated code.  See
   oak-native.h for how the pieces fit together. */

#define _REENTRANT


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "data.h"
#include "gc.h"
#include "stacks.h"
#include "xmalloc.h"
#include "native.h"

/* The embedded program's .oa text (oak-embed.h), or null. */
const char *oak_embedded_program = NULL;
size_t oak_embedded_program_size = 0;

/* Modules named by --native-module, loaded once the tables exist. */
static const char **pending_modules = NULL;
static unsigned pending_module_count = 0, pending_module_capacity = 0;

void
oak_native_module_pending(const char *path)
{
  if (pending_module_count == pending_module_capacity)
    {
      pending_module_capacity =
	pending_module_capacity ? 2 * pending_module_capacity : 64;
      pending_modules = realloc(pending_modules,
				pending_module_capacity
				* sizeof(*pending_modules));
      if (pending_modules == NULL)
	{
	  fprintf(stderr, "Out of memory noting --native-module options.\n");
	  exit(EXIT_FAILURE);
	}
    }
  pending_modules[pending_module_count++] = path;
}

void
oak_load_pending_native_modules(void)
{
  unsigned i;

  for (i = 0; i < pending_module_count; i++)
    if (!oak_native_load(pending_modules[i]))
      exit(EXIT_FAILURE);
}

#ifndef THREADS

#ifdef HAVE_DLOPEN
#include <dlfcn.h>
#endif

/* Every function of every loaded module, by name.  A name loaded
   again (a module recompiled and reloaded) takes over its old index,
   so stubs already pointing there call the new code. */

oak_native_fn *oak_native_fns = NULL;
static char **native_names = NULL;
static size_t native_count = 0, native_capacity = 0;

static size_t *native_hash = NULL;	/* index + 1, or 0 for empty */
static size_t native_hash_size = 0;

/* Stamped into a stub along with the index it resolved to.  A world
   dumped with resolved stubs and loaded into another process, with
   other modules or none, finds a different epoch and looks again. */
ref_t oak_native_epoch;

static size_t
hash_name(const char *s)
{
  size_t h = 5381;

  while (*s)
    h = h * 33 + (unsigned char)*s++;
  return h;
}

static size_t
lookup_slot(const char *name)
{
  size_t i = hash_name(name) & (native_hash_size - 1);

  while (native_hash[i] != 0
	 && strcmp(native_names[native_hash[i] - 1], name) != 0)
    i = (i + 1) & (native_hash_size - 1);
  return i;
}

static void
grow_hash(void)
{
  size_t i, old_size = native_hash_size;
  size_t *old = native_hash;

  native_hash_size = old_size ? old_size * 2 : 256;
  native_hash = xmalloc(native_hash_size * sizeof(size_t));
  memset(native_hash, 0, native_hash_size * sizeof(size_t));
  for (i = 0; i < old_size; i++)
    if (old[i] != 0)
      native_hash[lookup_slot(native_names[old[i] - 1])] = old[i];
  free(old);
}

static void
register_function(const char *name, oak_native_fn fn)
{
  size_t slot;

  if (2 * (native_count + 1) > native_hash_size)
    grow_hash();
  slot = lookup_slot(name);
  if (native_hash[slot] != 0)
    {
      oak_native_fns[native_hash[slot] - 1] = fn;
      return;
    }
  if (native_count == native_capacity)
    {
      native_capacity = native_capacity ? native_capacity * 2 : 256;
      oak_native_fns =
	realloc(oak_native_fns, native_capacity * sizeof(*oak_native_fns));
      native_names =
	realloc(native_names, native_capacity * sizeof(*native_names));
      if (oak_native_fns == NULL || native_names == NULL)
	{
	  fprintf(stderr, "Fatal error (vm): out of memory "
		  "registering native functions.\n");
	  exit(EXIT_FAILURE);
	}
    }
  oak_native_fns[native_count] = fn;
  native_names[native_count] = strdup(name);
  native_hash[slot] = ++native_count;
}

void
oak_native_init(void)
{
  size_t r = (size_t)time(NULL) * 2654435761u ^ (size_t)getpid();

  /* Any fixnum but zero; the top bits are dropped to make it one. */
  r &= ((size_t)1 << (OAK_WORD_SIZE - TAGSIZE - 1)) - 1;
  oak_native_epoch = INT_TO_REF(r | 1);
}

bool
oak_native_load(const char *path)
{
#ifdef HAVE_DLOPEN
  void *handle;
  const oak_native_module *m;

  /* A name without a slash is a file here, not a library for dlopen
     to search for. */
  if (strchr(path, '/') == NULL)
    {
      char *here = xmalloc(strlen(path) + 3);

      strcpy(here, "./");
      strcat(here, path);
      handle = dlopen(here, RTLD_NOW | RTLD_LOCAL);
      free(here);
    }
  else
    handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (handle == NULL)
    {
      fprintf(stderr, "Unable to load native module %s: %s\n",
	      path, dlerror());
      return false;
    }
  m = (const oak_native_module *)dlsym(handle, "oak_native_module_info");
  if (m == NULL)
    {
      fprintf(stderr, "%s is not an Oaklisp native module.\n", path);
      return false;
    }
  if (m->abi != OAK_NATIVE_ABI || m->word_size != OAK_WORD_SIZE
      || m->instrs_per_ref != INSTRS_PER_REF)
    {
      fprintf(stderr, "Native module %s was built for another emulator "
	      "(abi %u, %u-bit, %u instructions per ref; this is "
	      "abi %u, %u-bit, %u).\n", path,
	      m->abi, m->word_size, m->instrs_per_ref,
	      OAK_NATIVE_ABI, OAK_WORD_SIZE, INSTRS_PER_REF);
      return false;
    }
  oak_native_register(m);
  return true;
#else
  fprintf(stderr, "Unable to load native module %s: "
	  "this emulator was built without dlopen.\n", path);
  return false;
#endif
}

/* Every registered module's direct-call sites, for flushing. */

static const oak_native_module **site_modules = NULL;
static unsigned site_module_count = 0, site_module_capacity = 0;

void
oak_native_flush_sites(void)
{
  unsigned i, j;

  for (i = 0; i < site_module_count; i++)
    for (j = 0; j < site_modules[i]->site_count; j++)
      site_modules[i]->sites[j].epoch = INT_TO_REF(0);
}

/* Register a module's functions: one statically linked into an oakc
   executable, or one oak_native_load has checked.  The argument is
   const void * so the declaration needs none of this file's types. */

void
oak_native_register(const void *module_info)
{
  const oak_native_module *m = module_info;
  unsigned i;

  for (i = 0; i < m->count; i++)
    register_function(m->entries[i].name, m->entries[i].fn);

  /* dlopen of an unchanged path gives the module struct back, so don't
     list it twice. */
  for (i = 0; i < site_module_count; i++)
    if (site_modules[i] == m)
      break;
  if (i == site_module_count && m->site_count != 0)
    {
      if (site_module_count == site_module_capacity)
	{
	  site_module_capacity =
	    site_module_capacity ? 2 * site_module_capacity : 64;
	  site_modules = realloc(site_modules,
				 site_module_capacity
				 * sizeof(*site_modules));
	  if (site_modules == NULL)
	    {
	      fprintf(stderr, "Fatal error (vm): out of memory "
		      "registering native call sites.\n");
	      exit(EXIT_FAILURE);
	    }
	}
      site_modules[site_module_count++] = m;
    }

  /* Sites may hold functions this module's code replaces. */
  oak_native_flush_sites();
}

/* Fill the call site S for the operation V, which is on top of the
   value stack, with Y_TYPE the type of the argument under it (0 when
   the stack hid it): V's lambda method, or for a generic V the method
   its own one-entry cache holds for Y_TYPE -- only what the cache
   witnesses, never a search, so the site stays inside what the
   emulator's FUNCALL would have done.  A null fn means a call has to
   go the long way (not an operation, a method that is not native, or
   a generic aimed at another type right now, recorded as method e_nil
   so the site refills once the cache comes back to Y_TYPE).  The
   checks mirror oak_native_call_1 (oak-native.h). */

void
oak_native_site_fill(oak_native_site * s, ref_t v, ref_t y_type)
{
  ref_t m, seg;
  oak_native_fn fn;

  s->epoch = oak_native_epoch;
  s->op = v;
  s->y_type = INT_TO_REF(0);
  s->offset = INT_TO_REF(0);
  s->method = s->code = s->env = e_nil;
  s->fn = NULL;
  if (!TAG_IS(v, PTR_TAG))
    return;
  m = REF_SLOT(v, OPERATION_LAMBDA_OFF);
  if (m == e_false)
    {
      /* Generic. */
      if (y_type == INT_TO_REF(0))
	{
	  s->epoch = INT_TO_REF(0);	/* could not see the argument */
	  return;
	}
      s->y_type = y_type;
      if (REF_SLOT(v, OPERATION_CACHE_TYPE_OFF) != y_type)
	return;
      m = REF_SLOT(v, OPERATION_CACHE_METH_OFF);
      if (!TAG_IS(m, PTR_TAG) || REF_SLOT(m, 0) != e_method_type)
	return;
      s->method = m;
      s->offset = REF_SLOT(v, OPERATION_CACHE_TYPE_OFF_OFF);
    }
  else
    {
      if (!TAG_IS(m, PTR_TAG) || REF_SLOT(m, 0) != e_method_type)
	return;
      s->method = m;
    }
  seg = REF_SLOT(m, METHOD_CODE_OFF);
  if ((fn = oak_native_known(seg)) == NULL)
    return;
  s->code = seg;
  s->env = REF_SLOT(m, METHOD_ENV_OFF);
  s->fn = fn;
}

/* Find the C function for the stub code vector SEG and record it
   there, or return NULL if no loaded module has it. */
oak_native_fn
oak_native_resolve(ref_t seg)
{
  ref_t *cv = REF_TO_PTR(seg);
  size_t n = REF_TO_INT(cv[1]);
  ref_t name = cv[n - 5];
  ref_t *s;
  char *cname;
  size_t slot;

  if (!TAG_IS(name, PTR_TAG))
    return NULL;
  s = REF_TO_PTR(name);
  cname = oak_c_string(s + 3, (int)REF_TO_INT(s[2]));
  slot = native_hash_size ? lookup_slot(cname) : 0;
  if (native_hash_size == 0 || native_hash[slot] == 0)
    {
      fprintf(stderr, "Native function %s is not loaded.\n", cname);
      free(cname);
      return NULL;
    }
  free(cname);
  cv[n - 3] = INT_TO_REF(native_hash[slot] - 1);
  cv[n - 1] = oak_native_epoch;
  return oak_native_fns[native_hash[slot] - 1];
}

/* Build NAME.so from NAME.c. */
bool
oak_native_cc(const char *c_file, const char *so_file)
{
  const char *cc = getenv("OAKLISP_NATIVE_CC");
  const char *cflags = getenv("OAKLISP_NATIVE_CFLAGS");
  const char *incdir = getenv("OAKLISP_NATIVE_INCLUDE");
  char *command;
  size_t len;
  int status;

  /* The generated code includes oak-native.h, and with it this
     emulator's own headers and configuration, from the tree it was
     built in.  OAKLISP_NATIVE_INCLUDE replaces those flags. */
  if (cc == NULL)
    cc = OAK_NATIVE_CC;
  if (cflags == NULL)
    cflags = OAK_NATIVE_CFLAGS;
  if (incdir == NULL)
    incdir = OAK_NATIVE_INCLUDE;

  len = strlen(cc) + strlen(cflags) + strlen(incdir)
    + strlen(OAK_NATIVE_SHARED) + strlen(c_file) + strlen(so_file) + 32;
  command = xmalloc(len);
  snprintf(command, len, "%s %s %s %s -o '%s' '%s'",
	   cc, cflags, OAK_NATIVE_SHARED, incdir, so_file, c_file);
  if (trace_files)
    printf("Running %s\n", command);
  fflush(stdout);
  status = system(command);
  free(command);
  return status == 0;
}

/* The slow paths of the macros in oak-native.h. */

ref_t *
oak_native_unflush(ref_t * sp, int n)
{
  value_stack.sp = sp;
  stack_unflush(&value_stack, n);
  return value_stack.sp;
}

ref_t *
oak_native_flush(ref_t * sp)
{
  value_stack.sp = sp;
  stack_flush(&value_stack, value_stack.filltarget);
  return value_stack.sp;
}

ref_t *
oak_native_push_slow(ref_t * sp, ref_t v)
{
  value_stack.sp = sp;
  GC_MEMORY(v);
  stack_flush(&value_stack, value_stack.filltarget);
  GC_RECALL(v);
  *++value_stack.sp = v;
  return value_stack.sp;
}

ref_t *
oak_native_pop_n(ref_t * sp, long n)
{
  value_stack.sp = sp;
  stack_pop_n(&value_stack, n);
  return value_stack.sp;
}

void
oak_native_context_room(void)
{
  stack_flush(&context_stack, context_stack.filltarget);
}

/* Instructions that allocate, as the emulator does them.  The
   allocation can collect garbage, which moves things, so the operands
   are read from the stack only after it. */

ref_t *
oak_native_cons(ref_t * sp)
{
  ref_t *p;

  value_stack.sp = sp;
  ALLOCATE(p, 3, "space crunch in native CONS");
  sp = value_stack.sp;
  p[0] = e_cons_type;
  p[CONS_PAIR_CAR_OFF] = sp[0];
  p[CONS_PAIR_CDR_OFF] = sp[-1];
  *--sp = PTR_TO_REF(p);
  return sp;
}

ref_t *
oak_native_make_cell(ref_t * sp)
{
  ref_t *p;

  value_stack.sp = sp;
  ALLOCATE(p, 1, "space crunch in native MAKE-CELL");
  sp = value_stack.sp;
  *p = sp[0];
  sp[0] = PTR_TO_LOC(p);
  return sp;
}

ref_t *
oak_native_make_lambda(ref_t * sp)
{
  ref_t *p;

  value_stack.sp = sp;
  ALLOCATE(p, 8, "space crunch in native MAKE-LAMBDA");
  sp = value_stack.sp;
  p[0] = e_operation_type;
  p[OPERATION_LAMBDA_OFF] = PTR_TO_REF(p + OPERATION_LENGTH);
  p[OPERATION_CACHE_TYPE_OFF] = e_uninitialized;
  p[OPERATION_CACHE_METH_OFF] = e_uninitialized;
  p[OPERATION_CACHE_TYPE_OFF_OFF] = e_uninitialized;
  p[OPERATION_LENGTH] = e_method_type;
  p[OPERATION_LENGTH + METHOD_CODE_OFF] = sp[0];
  p[OPERATION_LENGTH + METHOD_ENV_OFF] = sp[-1];
  *--sp = PTR_TO_REF(p);
  return sp;
}

ref_t *
oak_native_make_closed_environment(ref_t * sp, int n)
{
  ref_t *p;
  int i;

  /* With no captured variables the net effect is a push, so make the
     room first: flushing can collect, which would move P. */
  if (n == 0 && sp + 1 >= value_stack.bp + value_stack.size)
    {
      value_stack.sp = sp;
      stack_flush(&value_stack, value_stack.filltarget);
      sp = value_stack.sp;
    }
  value_stack.sp = sp;
  ALLOCATE(p, (size_t)n + 2, "space crunch in native MAKE-CLOSED-ENVIRONMENT");
  sp = value_stack.sp;
  p[0] = e_env_type;
  p[1] = INT_TO_REF(n + 2);
  for (i = 0; i < n; i++)
    p[2 + i] = sp[-i];
  sp -= n - 1;
  sp[0] = PTR_TO_REF(p);
  return sp;
}

void
oak_native_bad_resume(unsigned k)
{
  fprintf(stderr, "Fatal error (vm): native function entered at "
	  "unknown resume point %u.\n", k);
  exit(EXIT_FAILURE);
}

/* Never called: the target of a direct call whose guard is the
   constant 0 (a same-file callee that stayed bytecode). */
int
oak_native_dead(unsigned k)
{
  (void)k;
  fprintf(stderr, "Fatal error (vm): a dead direct call ran.\n");
  exit(EXIT_FAILURE);
}

#else /* THREADS */

void
oak_native_init(void)
{
}

void
oak_native_flush_sites(void)
{
}

void
oak_native_register(const void *module_info)
{
  (void)module_info;
  fprintf(stderr, "Native code is not supported with threads.\n");
  exit(EXIT_FAILURE);
}

bool
oak_native_load(const char *path)
{
  fprintf(stderr, "Unable to load native module %s: "
	  "native code is not supported with threads.\n", path);
  return false;
}

bool
oak_native_cc(const char *c_file, const char *so_file)
{
  (void)c_file;
  (void)so_file;
  fprintf(stderr, "Native code is not supported with threads.\n");
  return false;
}

#endif
