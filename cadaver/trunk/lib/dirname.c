/* dirname.c -- return all but the last element in a path
   Copyright (C) 1990, 1998 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

/*******************************************************
 * THIS IS A MODIFIED dirname IMPLEMENTATION:
 * - sitecopy wants "" if there is no directory name,
 *   standard GNU implementation gives us "."
 * - sitecopy wants the trailing slash.
 *******************************************************/

#ifdef STDC_HEADERS
# include <stdlib.h>
#else
char *malloc ();
#endif
#if defined STDC_HEADERS || defined HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
# ifndef strrchr
#  define strrchr rindex
# endif
#endif

#include "dirname.h"

/* Return the leading directories part of PATH,
   allocated with malloc.  If out of memory, return 0.
   Assumes that trailing slashes have already been
   removed.  */

char *
dir_name (const char *path)
{
  char *newpath;
  char *slash;
  int length;			/* Length of result, not including NUL.  */

  slash = strrchr (path, '/');
  if (slash == 0)
    {
      /* File is in the current directory.  */
      path = "";
      length = 0;
    }
  else
    {
      /* Remove any trailing slashes from the result.
      while (slash > path && *slash == '/')
      --slash; */

      length = slash - path + 1;
    }
  newpath = (char *) malloc (length + 1);
  if (newpath == 0)
    return 0;
  strncpy (newpath, path, length);
  newpath[length] = 0;
  return newpath;
}
