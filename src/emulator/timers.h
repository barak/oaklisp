/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#ifndef _TIMERS_H_INCLUDED
#define _TIMERS_H_INCLUDED

/* the functions return milliseconds */

extern unsigned long get_real_time (void);
extern unsigned long get_user_time (void);

#endif
