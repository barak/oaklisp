/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#define _REENTRANT

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "config.h"
#include "timers.h"


#if defined(HAVE_GETTICKCOUNT)

unsigned long
get_real_time(void)
{
  return (unsigned long)GetTickCount();
}

unsigned long
get_user_time(void)
{
  return get_real_time();
}

#elif defined(HAVE_GETRUSAGE)

#include <sys/time.h>

#if (defined(__hpux) && !defined(_HPUX_SOURCE))
#define _HPUX_SOURCE
#endif

#if (defined(sun) && defined(__SVR4))
#include "/usr/ucbinclude/sys/rusage.h"
#include "/usr/ucbinclude/sys/resource.h"
#else
#include <sys/resource.h>
#endif

#ifdef __hpux
#include <sys/syscall.h>
#define     getrusage(a, b)     syscall(SYS_getrusage, (a), (b))
#endif

unsigned long
get_real_time(void)
{
  unsigned long result;
  struct timeval tnow;

  if (0 != gettimeofday(&tnow, 0))
    {
      fprintf(stderr, "ERROR (Time): Unable to obtain time of day; %s\n",
	      strerror(errno));
      exit(EXIT_FAILURE);
    }
  else
    {
      result = tnow.tv_sec * 1000
	+ tnow.tv_usec / 1000;
      return result;
    }
}


unsigned long
get_user_time(void)
{
  struct rusage rusage;
  unsigned long result;

  if (0 != getrusage(RUSAGE_SELF, &rusage))
    {
      fprintf(stderr, "ERROR (Time): Unable to getrusage(); %s\n",
	      strerror(errno));
      exit(EXIT_FAILURE);
    }
  else
    {
      result = rusage.ru_utime.tv_sec * 1000
	+ rusage.ru_utime.tv_usec / 1000;
      return result;
    }
}

#else /* plain ansi-libraries */

unsigned long
get_real_time(void)
{
#ifdef __GLIBC_HAVE_LONG_LONG
  unsigned long long temp;
  temp = (unsigned long long)clock()
    * (1000ull / CLOCKS_PER_SEC);
#else
  long temp;
  temp = (clock() * 1000) / (CLOCKS_PER_SEC);
#endif
  /* caution: the clock() function on some systems with a high
     frequency clock (e.g. transputers ) give values
     modulo a not too big time span, so a wrap around
     can occur between to calls, which leads to odd results
   */
  return (unsigned long)temp;
}

unsigned long
get_user_time(void)
{
  return get_real_time();
}

#endif
