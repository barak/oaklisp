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
#include <setjmp.h>
#include "threads.h"
#include "xmalloc.h"
#include "stacks.h"
#include "loop.h"
#include "gc.h"
#ifdef USE_MARK_SWEEP
#include "gc-ms.h"
#endif

#ifdef THREADS
int next_index = 0;
oak_tls_key_t index_key;
oak_mutex_t gc_lock = OAK_MUTEX_INITIALIZER;
oak_mutex_t alloc_lock = OAK_MUTEX_INITIALIZER;
oak_mutex_t index_lock = OAK_MUTEX_INITIALIZER;
oak_mutex_t test_and_set_locative_lock = OAK_MUTEX_INITIALIZER;
bool gc_pending = false;
int gc_ready[MAX_THREAD_COUNT];
/* Slots of threads that have exited.  next_index never decreases, so
   without this a dead thread's gc_ready flag would stay 0 forever and
   stall every subsequent stop-the-world handshake. */
int gc_thread_dead[MAX_THREAD_COUNT];
register_set_t* register_array[MAX_THREAD_COUNT];
oakstack *value_stack_array[MAX_THREAD_COUNT];
oakstack *cntxt_stack_array[MAX_THREAD_COUNT];
#endif

#ifdef THREADS
static instr_t tail_recurse_instruction = (22 << 2);

/* Unwind targets for threads whose thunk runs to completion.  A spawned
   thread starts with an empty context stack, so the RETURN that leaves
   the thunk's outermost frame has no context to pop; stack_unflush
   detects that and unwinds back to init_thread instead of walking a
   segment list that isn't there.  Slot 0, the main thread, is never
   armed: when the boot code returns there is nowhere to unwind to. */
static jmp_buf thread_exit_point[MAX_THREAD_COUNT];
static volatile int thread_exit_armed[MAX_THREAD_COUNT];

/* True when the calling thread can be unwound out of loop(). */
int
oak_thread_can_exit(void)
{
  int *my_index_p = (int *)oak_tls_get(index_key);

  if (my_index_p == NULL)
    return 0;
  if (*my_index_p <= 0 || *my_index_p >= MAX_THREAD_COUNT)
    return 0;
  return thread_exit_armed[*my_index_p];
}

/* Leave the interpreter and return to init_thread.  Does not return. */
void
oak_thread_exit_unwind(void)
{
  int my_index = *((int *)oak_tls_get(index_key));

  /* This thread will not execute another instruction, so retire it here
     rather than making a collection that is already waiting on our
     handshake flag wait for the thread-local destructor to run.
     free_registers repeats this when the thread actually exits. */
  gc_thread_dead[my_index] = 1;
  gc_ready[my_index] = 1;

  thread_exit_armed[my_index] = 0;
  longjmp(thread_exit_point[my_index], 1);
}

void
oak_threads_system_init(void)
{
#ifdef OAK_NEEDS_DYNAMIC_MUTEX_INIT
    oak_mutex_init(&gc_lock);
    oak_mutex_init(&alloc_lock);
    oak_mutex_init(&index_lock);
    oak_mutex_init(&test_and_set_locative_lock);
    {
      extern oak_mutex_t wp_lock;
      extern oak_mutex_t dump_lock;
      oak_mutex_init(&wp_lock);
      oak_mutex_init(&dump_lock);
    }
#endif
}
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
  oak_thread_t new_thread;
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
  gc_thread_dead[index] = 0;
  info_p->start_operation = start_operation;
  info_p->parent_index = *((int *)oak_tls_get(index_key));
  info_p->my_index = index;
  if (oak_thread_create(&new_thread, init_thread, (void *)info_p)) {
    free(info_p);
    return 0;
  }
  else
    return 1;
#else
  return 0;
#endif
}

#ifdef THREADS
static void *init_thread (void *info_p)
{
  volatile int my_index;
  int *my_index_p;
  start_info_t info;
  my_index_p = (int *)malloc(sizeof(int));
  info = *((start_info_t *)info_p);
  free(info_p);
  /* Retrieve the next index in the thread arrays and lock it so
     another starting thread cannot get the same index */

  *my_index_p = info.my_index;
  my_index = *my_index_p;
  oak_tls_set(index_key, (void *)my_index_p);
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

#ifdef USE_MARK_SWEEP
  tlab_cursor_array[my_index] = NULL;
  tlab_end_array[my_index] = NULL;
#endif

  /* At this point, it should be OK if the garbage collector gets run. */
  e_pc = &tail_recurse_instruction;
  e_nargs = 0;

  /* Big virtual machine interpreter loop.  It never returns: a thunk
     that runs to completion returns from its outermost frame, which
     underflows this thread's context stack, and stack_unflush unwinds
     back to the setjmp below instead of faulting. */
  if (setjmp(thread_exit_point[my_index]) == 0)
    {
      thread_exit_armed[my_index] = 1;
      loop(info.start_operation);
    }

  thread_exit_armed[my_index] = 0;

  fprintf(stderr, "Warning: heavyweight thread %d thunk returned; thread exiting.\n",
	  my_index);
  return 0;
}
#endif

void set_gc_flag (bool flag)
{
#ifdef THREADS
  if (flag == true) {
    oak_mutex_lock(&gc_lock);
    gc_pending = flag;
  }
  else {
    gc_pending = flag;
    oak_mutex_unlock(&gc_lock);
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
  oak_mutex_lock(&index_lock);
  if (next_index >= MAX_THREAD_COUNT) {
    ret = -1;
  } else {
    ret = next_index;
    next_index++;
  }
  oak_mutex_unlock(&index_lock);
#endif
  return (ret);
}

/* Thread-local storage destructor for index_key: runs when a thread
   exits.  Retires the thread's slot so the garbage collector stops
   waiting for it and stops scanning its stacks as roots. */

void free_registers (void *arg)
{
#ifdef THREADS
  int *my_index_p = (int *)arg;
  int i;

  if (my_index_p == NULL)
    return;

  i = *my_index_p;
  if (i < 0 || i >= MAX_THREAD_COUNT)
    return;

  /* Announce the slot is gone before doing anything that can block, so
     a collection that is already waiting on our handshake flag is free
     to finish. */
  gc_thread_dead[i] = 1;
  gc_ready[i] = 1;

  /* gc_lock is held for the duration of a collection, so taking it
     here guarantees nobody is scanning our stacks right now.  Anyone
     who starts afterwards will skip the slot. */
  oak_mutex_lock(&gc_lock);

  if (value_stack_array[i] != NULL)
    {
      if (value_stack_array[i]->bp != NULL)
	free(value_stack_array[i]->bp - 1);
      free(value_stack_array[i]);
      value_stack_array[i] = NULL;
    }
  if (cntxt_stack_array[i] != NULL)
    {
      if (cntxt_stack_array[i]->bp != NULL)
	free(cntxt_stack_array[i]->bp - 1);
      free(cntxt_stack_array[i]);
      cntxt_stack_array[i] = NULL;
    }
  if (register_array[i] != NULL)
    {
      free(register_array[i]);
      register_array[i] = NULL;
    }

  oak_mutex_unlock(&gc_lock);

  free(my_index_p);
#else
  (void)arg;
#endif
}

void wait_for_gc()
{
#ifdef THREADS
  int *my_index_p;
  int  my_index;
  my_index_p = oak_tls_get(index_key);
  my_index = *(my_index_p);
  gc_ready[my_index] = 1;
  oak_mutex_lock(&gc_lock);
  gc_ready[my_index] = 0;
  oak_mutex_unlock(&gc_lock);
#endif
}
