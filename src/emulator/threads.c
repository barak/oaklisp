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


#define _REENTRANT

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "threads.h"
#include "xmalloc.h"
#include "stacks.h"
#include "loop.h"
#include "gc.h"

#ifdef THREADS
int next_index = 0;
pthread_key_t index_key;
pthread_mutex_t gc_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t alloc_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t index_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t test_and_set_locative_lock = PTHREAD_MUTEX_INITIALIZER;
bool gc_pending = false;
int gc_ready[MAX_THREAD_COUNT];
register_set_t* register_array[MAX_THREAD_COUNT];
oakstack *value_stack_array[MAX_THREAD_COUNT];
oakstack *cntxt_stack_array[MAX_THREAD_COUNT];
#endif

#ifdef THREADS
static u_int16_t tail_recurse_instruction = (22 << 2);
#endif

typedef struct {
  ref_t start_operation;
  int parent_index;
  int my_index;
} start_info_t;

#ifdef THREADS
static void *init_thread(void *info_p);
#endif

int create_thread(ref_t start_operation)
{
#ifdef THREADS
  pthread_t new_thread;
  int index;
  start_info_t *info_p = (start_info_t *)malloc(sizeof(start_info_t));
  index = get_next_index();
  if (index == -1) {
    fprintf (stderr,
	     "Max thread count of %d has been exceeded.  No thread created\n",
	     MAX_THREAD_COUNT);
    return 0;
  }
  gc_ready[index] = 0;
  info_p->start_operation = start_operation;
  info_p->parent_index = *((int *)pthread_getspecific(index_key));
  info_p->my_index = index;
  if (pthread_create(&new_thread, NULL,
		     (void *)init_thread, (void *)info_p))
    // Error creating --- need to add some clean up code here !!!
    return 0;
  else
    return 1;
#else
  return 0;
#endif
}

#ifdef THREADS
static void *init_thread (void *info_p)
{
  int my_index;
  int *my_index_p;
  start_info_t info;
  my_index_p = (int *)malloc(sizeof(int));
  info = *((start_info_t *)info_p);
  free(info_p);
  /* Retrieve the next index in the thread arrays and lock it so
     another starting thread cannot get the same index */

  *my_index_p = info.my_index;
  my_index = *my_index_p;
  pthread_setspecific(index_key, (void *)my_index_p);
  /* Increment also releases the gc lock on next_index so another
     starting thread can get the lock, or a thread that is gc'ing can
     get the lock */

  /* Shouldn't get interrupted for gc until after stacks are
     created.  This is below here in the vm not checking intterupts
     until after we get to the loop */

  value_stack_array[my_index] = (oakstack*)malloc (sizeof (oakstack));
  cntxt_stack_array[my_index] = (oakstack*)malloc(sizeof (oakstack));

  value_stack_array[my_index]->size = value_stack_array[0]->size;
  value_stack_array[my_index]->filltarget = value_stack_array[0]->filltarget;
  cntxt_stack_array[my_index]->size = cntxt_stack_array[0]->size;
  cntxt_stack_array[my_index]->filltarget = cntxt_stack_array[0]->filltarget;

  init_stacks ();
  register_array[my_index] = (register_set_t*)malloc(sizeof (register_set_t));

  memcpy(register_array[my_index], register_array[info.parent_index],
	 sizeof(register_set_t));

  gc_examine_ptr = gc_examine_buffer;

  /* At this point, it should be OK if the garbage collector gets run. */
  e_pc = &tail_recurse_instruction;
  e_nargs = 0;

  /* Big virtual machine interpreter loop */
  loop(info.start_operation);

  return 0;
}
#endif

void set_gc_flag (bool flag)
{
#ifdef THREADS
  int *my_index_p;
  int  my_index;
  my_index_p = pthread_getspecific (index_key);
  my_index = *(my_index_p);

  if (flag == true) {
    pthread_mutex_lock (&gc_lock);
    gc_pending = flag;
  }
  else {
    gc_pending = flag;
    pthread_mutex_unlock (&gc_lock);
  }
#endif
}

/* Increment uses the gc lock since we must be sure that a new thread
   does not get started and begin processing while the gc is already
   running.  The get_next_index additionally ensures that no two
   threads get the same index when starting */

int get_next_index ()
{
  int ret = -1;
#ifdef THREADS
  pthread_mutex_lock (&index_lock);
  if (next_index >= MAX_THREAD_COUNT) {
    ret = -1;
  } else {
    ret = next_index;
    next_index++;
  }
  pthread_mutex_unlock (&index_lock);
#endif
  return (ret);
}

void free_registers ()
{
}

void wait_for_gc()
{
#ifdef THREADS
  int *my_index_p;
  int  my_index;
  my_index_p = pthread_getspecific (index_key);
  my_index = *(my_index_p);
  gc_ready[my_index] = 1;
  pthread_mutex_lock (&gc_lock);
  gc_ready[my_index] = 0;
  pthread_mutex_unlock (&gc_lock);
#endif
}
