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


/* Method search and the little accessors it is built on, shared by the
   interpreter (loop.c) and natively compiled code (native-ops.c), so
   that a stand-alone executable can link without the interpreter. */

#ifndef _DISPATCH_H_INCLUDED
#define _DISPATCH_H_INCLUDED

#include "config.h"
#include "data.h"

static inline ref_t
get_type(ref_t x)
{
  if (x & 0x1)
    {
      if (x & 0x2)
	return REF_SLOT(x, 0);
      else
	return *(e_subtype_table + ((x & SUBTAG_MASK) / 4));
    }
  else
    {
      if (x & 0x2)
	return e_loc_type;
      else
	return e_fixnum_type;
    }
}

static inline ref_t *
pcar(ref_t x)
{
  return &REF_SLOT(x, CONS_PAIR_CAR_OFF);
}

static inline ref_t *
pcdr(ref_t x)
{
  return &REF_SLOT(x, CONS_PAIR_CDR_OFF);
}

static inline ref_t
car(ref_t x)
{
  return *pcar(x);
}

static inline ref_t
cdr(ref_t x)
{
  return *pcdr(x);
}

/* ASSQ, refusing to run off a malformed list: false means ELT was not
   found and the list was malformed; otherwise *RESULT is the pair or
   NOTFOUND. */
extern bool assq_checked(ref_t elt, ref_t lis, ref_t notfound,
			 ref_t * result);

static inline ref_t
assqcdr(ref_t elt, ref_t lis, ref_t notfound)
{
  ref_t this;

  while (lis != e_nil)
    {
      this = car(lis);
      if (car(this) == elt)
	return cdr(this);
      lis = cdr(lis);
    }
  return notfound;
}

static inline int
lookup_bp_offset(ref_t y_type, ref_t meth_type)
{
  return assqcdr(meth_type,
		 REF_SLOT(y_type, TYPE_TYPE_BP_ALIST_OFF),
		 INT_TO_REF(0));
}

extern void find_method_type_pair(ref_t op, ref_t obj_type,
				  ref_t * method_ptr, ref_t * type_ptr);

#ifndef THREADS
extern ref_t oak_native_find_method(ref_t op, ref_t y_type, ref_t * offset);
#endif

#endif
