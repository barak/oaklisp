#ifndef _THREADS_H_INCLUDED
#define _THREADS_H_INCLUDED

#include <pthread.h>
#include "config.h"

#ifdef THREADS
extern int next_index;
extern pthread_key_t index_key;
extern pthread_mutex_t index_lock;
extern pthread_mutex_t alloc_lock;
extern pthread_mutex_t testandsetcar_lock;
#endif



#ifdef THREADS
#define THREADY(x) x
#else
#define THREADY(x)
#endif

#endif /*_THREADS_H_INCLUDED*/
