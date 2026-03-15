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

/*
 * oak-getopt.h - Portable getopt_long_only.
 *
 * On POSIX systems (Linux, macOS, MinGW) this simply includes
 * <getopt.h>.  On MSVC, which lacks getopt entirely, a minimal
 * self-contained implementation is provided.
 */

#ifndef OAK_GETOPT_H_INCLUDED
#define OAK_GETOPT_H_INCLUDED

#if !defined(_MSC_VER) || defined(__MINGW32__)

/* POSIX / MinGW — use the system getopt. */
#include <getopt.h>

#else /* MSVC */

#include <string.h>
#include <stdio.h>

#ifndef no_argument
#define no_argument       0
#define required_argument 1
#define optional_argument 2
#endif

struct option {
    const char *name;
    int         has_arg;
    int        *flag;
    int         val;
};

static char *optarg;
static int   optind = 1;

/*
 * Minimal getopt_long_only: handles --option and -option long options.
 * Short option string is ignored (assumed empty).
 * Returns option val, 0 for flag options, '?' on error, -1 when done.
 */
static int
getopt_long_only(int argc, char *const argv[],
                 const char *shortopts,
                 const struct option *longopts,
                 int *longindex)
{
    const char *arg;
    const char *eq;
    size_t namelen;
    int i;
    (void)shortopts;

    optarg = NULL;

    while (optind < argc) {
        arg = argv[optind];

        /* Stop at "--" or first non-option argument. */
        if (arg[0] != '-')
            return -1;
        if (arg[0] == '-' && arg[1] == '-' && arg[2] == '\0') {
            optind++;
            return -1;
        }

        /* Skip leading dashes (accept both -opt and --opt). */
        arg++;
        if (*arg == '-')
            arg++;

        /* Check for --option=value form. */
        eq = strchr(arg, '=');
        namelen = eq ? (size_t)(eq - arg) : strlen(arg);

        for (i = 0; longopts[i].name; i++) {
            if (strncmp(longopts[i].name, arg, namelen) == 0
                && longopts[i].name[namelen] == '\0') {

                optind++;

                if (longopts[i].has_arg == required_argument) {
                    if (eq) {
                        optarg = (char *)(eq + 1);
                    } else if (optind < argc) {
                        optarg = argv[optind++];
                    } else {
                        fprintf(stderr, "%s: option '--%s' requires"
                                " an argument\n", argv[0],
                                longopts[i].name);
                        return '?';
                    }
                } else if (eq) {
                    /* no_argument but '=' was given */
                    fprintf(stderr, "%s: option '--%s' doesn't allow"
                            " an argument\n", argv[0],
                            longopts[i].name);
                    return '?';
                }

                if (longindex)
                    *longindex = i;

                if (longopts[i].flag) {
                    *longopts[i].flag = longopts[i].val;
                    return 0;
                }
                return longopts[i].val;
            }
        }

        /* Unknown option. */
        fprintf(stderr, "%s: unrecognized option '%s'\n",
                argv[0], argv[optind]);
        optind++;
        return '?';
    }
    return -1;
}

#endif /* _MSC_VER */
#endif /* OAK_GETOPT_H_INCLUDED */
