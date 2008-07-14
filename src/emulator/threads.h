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


#ifndef _THREADS_H_INCLUDED
#define _THREADS_H_INCLUDED

#include <pthread.h>
#include "config.h"

#ifdef THREADS
extern int next_index;
extern pthread_key_t index_key;
extern pthread_mutex_t index_lock;
extern pthread_mutex_t alloc_lock;
extern pthread_mutex_t test_and_set_locative_lock;
#endif



#ifdef THREADS
#define THREADY(x) x
#else
#define THREADY(x)
#endif

#endif /*_THREADS_H_INCLUDED*/
