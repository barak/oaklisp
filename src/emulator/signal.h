/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/


#ifndef _SIGNAL_H_INCLUDED
#define _SIGNAL_H_INCLUDED

#ifndef _ICC

void enable_signal_polling (void);
void disable_signal_polling (void);
void clear_signal (void);
extern int signal_poll_flag;

#endif
#endif
