#include "threads.h"
#include "xmalloc.h"
#include "stacks.h"
#include "loop.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifdef THREADS
int next_index = 0;
pthread_key_t index_key;
pthread_mutex_t gc_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t alloc_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t index_lock = PTHREAD_MUTEX_INITIALIZER;
bool gc_pending = false;
int gc_ready[MAX_THREAD_COUNT];
register_set_t* register_array[MAX_THREAD_COUNT];
stack_t *value_stack_array[MAX_THREAD_COUNT];
stack_t *cntxt_stack_array[MAX_THREAD_COUNT];
#endif

static u_int16_t tail_recurse_instruction = (22 << 2);

typedef struct {
    ref_t start_operation;
    int parent_index;
    int my_index;
} start_info_t;

static void *init_thread(void *info_p);

int create_thread(ref_t start_operation)
{
#ifdef THREADS
    pthread_t new_thread;
    int index;
    start_info_t *info_p = (start_info_t *)malloc(sizeof(start_info_t));
    index = get_next_index();
    if (index == -1) {
	fprintf (stderr, "Max thread count of %d has been exceeded.  No thread created\n", MAX_THREAD_COUNT);
	return 0;
    }
    gc_ready[index] = 0;
    info_p->start_operation = start_operation;
    info_p->parent_index = *((int *)pthread_getspecific(index_key));
    info_p->my_index = index;
    pthread_create(&new_thread, NULL,
		   (void *)init_thread, (void *)info_p);
    return 1;
#else
    return 0;
#endif
}

static void *init_thread (void *info_p)
{
#ifdef THREADS
   int my_index;
   int *my_index_p;
   ref_t *p;
   start_info_t info;
   my_index_p = (int *)malloc (sizeof (int));
   info = *((start_info_t *)info_p);
   free(info_p);
   /*Retrieve the next index in the thread arrays and lock it so
     another starting thread cannot get the same index*/
  
   *my_index_p = info.my_index;
   my_index = *my_index_p;
   pthread_setspecific(index_key, (void *)my_index_p);  
   /*Increment also releases the gc lock on next_index so another
     starting thread can get the lock, or a thread that is gc'ing can
     get the lock*/

   /* Shouldn't get interrupted for gc until after stacks are
      created.  This is below here in the vm not checking intterupts
      until after we get to the loop */
   
   value_stack_array[my_index] = (stack_t*)malloc (sizeof (stack_t));
   cntxt_stack_array[my_index] = (stack_t*)malloc(sizeof (stack_t));

   value_stack_array[my_index]->size = value_stack_array[0]->size;
   value_stack_array[my_index]->filltarget = value_stack_array[0]->filltarget;
   cntxt_stack_array[my_index]->size = cntxt_stack_array[0]->size;
   cntxt_stack_array[my_index]->filltarget = cntxt_stack_array[0]->filltarget;
 
   init_stacks ();
   register_array[my_index] = (register_set_t*)malloc(sizeof (register_set_t));

   memcpy(register_array[my_index], register_array[info.parent_index],
	  sizeof(register_set_t));
   e_pc = &tail_recurse_instruction;
   *++value_stack.sp = info.start_operation;
 /*At this point, it should be OK if the garbage collector gets run.*/
   e_nargs = 0;
 /*ALLOCATE(e_pc, 1, "creating initial jump instruction");*/
 /*
   ALLOCATE(p, 4, "creating initial jump instruction");
   e_code_segment = PTR_TO_REF(p);
   e_pc = (u_int16_t *)(&p[3]);
   *e_pc = (22 << 2);
   */

   /* Big virtual machine interpreter loop */
   loop();

#endif
  
   return 0;
}

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

/*Increment uses the gc lock
  since we must be sure that a new thread does not
  get started and begin processing while the gc is
  already running.  The get_next_index additionally
  ensures that no two threads get the same index when
  starting*/
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
