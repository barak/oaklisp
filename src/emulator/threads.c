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
#endif

static void *init_thread (void *start_method)
{
#ifdef THREADS
   int my_index;
   int *my_index_p;
   my_index_p = (int *)xmalloc (sizeof (int));
   *my_index_p = next_index;
   my_index = next_index;
   pthread_setspecific(index_key, (void *)my_index_p);
   next_index++;
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
   loop();

#endif
   return 0;
}

int create_thread(ref_t start_method)
{
    pthread_t new_thread;
    pthread_create(&new_thread, NULL,
		   (void *)init_thread, (void *)&start_method);
    return 1;
}

