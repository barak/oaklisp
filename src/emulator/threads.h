#ifndef _THREADS_H_INCLUDED
#define _THREADS_H_INCLUDED

#include <pthread.h>
#include "config.h"
#include "data.h"

#ifdef THREADS
extern int next_index;
extern pthread_key_t index_key;
#endif

int create_thread(ref_t start_method);

#endif /*_THREADS_H_INCLUDED*/
