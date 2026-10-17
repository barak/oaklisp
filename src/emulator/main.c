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


/* The plain emulator's main.  It lives alone in this file so that
   liboakrt.a carries everything but main(), and an executable built by
   oakc can bring its own (see oak-embed.h). */

#include <stddef.h>
#include "oak-embed.h"

int
main(int argc, char **argv)
{
  return oak_main(argc, argv, NULL);
}
