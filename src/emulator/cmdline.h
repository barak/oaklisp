/**********************************************************************
 *     Copyright (c) by Barak Pearlmutter and Kevin Lang, 1987-99.    *
 *     Copyright (c) by Alex Stuebinger, 1998-99.                     *
 *     Distributed under the GNU General Public License v2 or later   *
 **********************************************************************/

#ifndef _CMDLINE_H_INCLUDED
#define _CMDLINE_H_INCLUDED

extern void parse_cmd_line (int argc, char **argv);
extern int program_arg_char(int arg_index, int char_index);

#endif
