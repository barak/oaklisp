#include "threads.h"
#include "xmalloc.h"
#include "stacks.h"
#include "loop.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#ifdef THREADS
int next_index = 0;
pthread_key_t index_key;
pthread_mutex_t gc_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t alloc_lock = PTHREAD_MUTEX_INITIALIZER;
bool gc_pending = false;
int gc_ready[200];
register_set_t* register_array[200];
stack_t *value_stack_array[200];
stack_t *cntxt_stack_array[200];
#endif

static void *init_thread (void *start_method)
{
#ifdef THREADS
   int my_index;
   int *my_index_p;
   my_index_p = (int *)xmalloc (sizeof (int));
   *my_index_p = next_index;
   my_index = *my_index_p;
   pthread_setspecific(index_key, (void *)my_index_p);
   /* Shouldn't get interrupted for gc until after stacks are
      created.  This is below here in the vm not checking intterupts
      until after we get to the loop*/
   gc_ready[my_index] = 0;
   inc_next_index();
   value_stack_array[my_index] = (stack_t*)xmalloc (sizeof (stack_t));
   cntxt_stack_array[my_index] = (stack_t*)xmalloc(sizeof (stack_t));

   value_stack_array[my_index]->size = value_stack_array[0]->size;
   value_stack_array[my_index]->filltarget = value_stack_array[0]->filltarget;
   cntxt_stack_array[my_index]->size = cntxt_stack_array[0]->size;
   cntxt_stack_array[my_index]->filltarget = cntxt_stack_array[0]->filltarget;
 
   init_stacks ();
   register_array[my_index] = (register_set_t*)xmalloc(sizeof (register_set_t));

   e_current_method = (ref_t)start_method;
   e_env = REF_TO_PTR (REF_SLOT (e_current_method, METHOD_ENV_OFF));
   e_code_segment = REF_SLOT (e_current_method, METHOD_CODE_OFF);
   e_pc = CODE_SEG_FIRST_INSTR (e_code_segment);
   e_bp = e_env;
   e_nargs = 0;

   /* Big virtual machine interpreter loop */
   /* loop();*/

#endif
   return 0;
}

int create_thread(ref_t start_method)
{
#ifdef THREADS
    pthread_t new_thread;
    pthread_create(&new_thread, NULL,
		   (void *)init_thread, (void *)&start_method);
#endif
    return 1;
}
void set_gc_flag (bool flag) 
{
#ifdef THREADS
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
void inc_next_index ()
{
#ifdef THREADS
  pthread_mutex_lock (&gc_lock);
  next_index++;
  pthread_mutex_unlock (&gc_lock);
#endif
}

