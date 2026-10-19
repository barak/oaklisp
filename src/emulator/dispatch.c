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


/* Method search, shared by the interpreter and natively compiled
   code; moved out of loop.c so that a stand-alone executable links
   without the interpreter.  See dispatch.h. */

#define _REENTRANT

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "data.h"
#include "xmalloc.h"
#include "dispatch.h"

#ifdef FAST
#define maybe_put(x, s)
#else
static inline void
maybe_put(bool v, char *s)
{
  if (v)
    {
      printf("%s", s);
      fflush(stdout);
    }
}
#endif


/* The ASSQ instruction takes its list off the value stack, so both the
   spine and every element of it are ordinary Oaklisp values that the
   program chose.  car() and cdr() dereference without looking at the
   tag, so this checks each cell the way CONSINSTR checks the cons
   instructions -- a pointer, and a pair -- before following it.  A
   false return means the instruction should trap; *RESULT is set only
   on success. */

bool
assq_checked(ref_t elt, ref_t lis, ref_t notfound, ref_t *result)
{
  while (lis != e_nil) {
    ref_t this;

    if (!TAG_IS(lis, PTR_TAG) || REF_SLOT(lis, 0) != e_cons_type)
      return false;
    this = car(lis);
    if (!TAG_IS(this, PTR_TAG) || REF_SLOT(this, 0) != e_cons_type)
      return false;
    if (car(this) == elt) {
      *result = this;
      return true;
    }
    lis = cdr(lis);
  }
  *result = notfound;
  return true;
}



/* Depth of the supertype search stack that lives on the C stack.  6 is
   enough for the current build, but a user-defined type graph can be
   arbitrarily deep, so past this point the table moves to the heap
   instead of running off the end of the frame. */
#define N_LATERS 64

void
find_method_type_pair(ref_t op,
		      ref_t obj_type,
		      ref_t *method_ptr,
		      ref_t *type_ptr)
{
  ref_t alist;
  ref_t car_cache;
  ref_t *locl = NULL;
#ifdef OP_METH_ALIST_MTF
  ref_t thelist;
  ref_t *loclist;
#endif
  /* stack of lists of types that remain to be searched */
  ref_t inline_lists[N_LATERS];
  ref_t *later_lists = inline_lists;
  size_t later_size = N_LATERS;
  size_t llp = 0;		/* index of first empty slot in table */

  while (1)			/* forever */
    {
      /* First look for it in the local method alist of obj_type: */


#ifdef OP_METH_ALIST_MTF
      alist = thelist =
	*(loclist = &REF_SLOT(obj_type, TYPE_OP_METHOD_ALIST_OFF));
#else
      alist = REF_SLOT(obj_type, TYPE_OP_METHOD_ALIST_OFF);
#endif
      while (alist != e_nil)
	{
	  if (car((car_cache = car(alist))) == op)
	    {
	      maybe_put(trace_meth, "x\n");
#ifdef OP_METH_ALIST_MTF
	      if (locl != NULL)
		{
		  *locl = cdr(alist);
		  *loclist = alist;
		  *pcdr(alist) = thelist;
		}
#endif
	      *method_ptr = cdr(car_cache);
	      *type_ptr = obj_type;
	      if (later_lists != inline_lists)
		free(later_lists);
	      return;
	    }
	  alist = *(locl = pcdr(alist));
	  maybe_put(trace_meth, "-");
	}

      /* Not found in local alist, so stack the entire supertype list
         and then fetch the top guy available on the stack. */

      if (llp == later_size)
	{
	  /* Grow the table; the first growth moves it off the C stack. */
	  size_t new_size = 2 * later_size;
	  ref_t *bigger;

	  if (later_lists == inline_lists)
	    {
	      bigger = (ref_t *) xmalloc(new_size * sizeof(ref_t));
	      memcpy(bigger, inline_lists, later_size * sizeof(ref_t));
	    }
	  else
	    {
	      bigger = (ref_t *) realloc(later_lists,
					 new_size * sizeof(ref_t));
	      if (bigger == NULL)
		{
		  fprintf(stderr, "\nERROR: out of memory while searching"
			  " a type hierarchy %lu levels deep.\n",
			  (unsigned long)later_size);
		  exit(EXIT_FAILURE);
		}
	    }
	  later_lists = bigger;
	  later_size = new_size;
	}
      later_lists[llp++] = REF_SLOT(obj_type, TYPE_SUPER_LIST_OFF);

      while (later_lists[llp - 1] == e_nil)
	{
	  if (llp == 1)
	    {
	      if (later_lists != inline_lists)
		free(later_lists);
	      return;
	    }
	  llp -= 1;
	}

      locl = NULL;
      obj_type = car(later_lists[llp - 1]);
      later_lists[llp - 1] = cdr(later_lists[llp - 1]);
    }
}




#ifndef THREADS
/* For natively compiled code (oak-native.h): find the method as
   FUNCALL does, filling the operation's method cache. */
ref_t
oak_native_find_method(ref_t op, ref_t y_type, ref_t *offset)
{
  /* Left alone when there is no method. */
  ref_t method = e_nil, meth_type;

  find_method_type_pair(op, y_type, &method, &meth_type);
  if (!is_method(method))
    return e_nil;
  *offset = lookup_bp_offset(y_type, meth_type);
#ifdef OP_TYPE_METH_CACHE
  REF_SLOT(op, OPERATION_CACHE_TYPE_OFF) = y_type;
  REF_SLOT(op, OPERATION_CACHE_METH_OFF) = method;
  REF_SLOT(op, OPERATION_CACHE_TYPE_OFF_OFF) = *offset;
#endif
  return method;
}
#endif

