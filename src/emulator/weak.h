/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/


#ifndef _WEAK_H_INCLUDED
#define _WEAK_H_INCLUDED

#include "data.h"

void init_weakpointer_tables(void);
void rebuild_wp_hashtable(void);
ref_t ref_to_wp(ref_t r);
extern unsigned long post_gc_wp(void);

#endif
