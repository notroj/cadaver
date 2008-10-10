/* 
   Common definitions for cadaver.
   Copyright (C) 1998-2005, Joe Orton <joe@orton.demon.co.uk>.
                                                                     
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef COMMON_H
#define COMMON_H

#include "config.h"

#include <sys/types.h>

#include <stdio.h>

#include <ne_utils.h>

#define DEBUG_FILES (1<<10)

/* A signal hander */
typedef void (*sig_handler)( int );

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifdef __EMX__
/* siebert: strcasecmp is stricmp */
#define strcasecmp stricmp
#endif

/* safe an extra header for lib/yesno.c exports. */
int yesno(void);

int cad_mkstemp(char *template);

/* boolean */
#define true 1
#define false 0

#if defined (__EMX__) || defined(__CYGWIN__)
#define FOPEN_BINARY_FLAGS "b"
#define OPEN_BINARY_FLAGS O_BINARY
#else
#define FOPEN_BINARY_FLAGS ""
#define OPEN_BINARY_FLAGS 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE (0)
#endif

#if !HAVE_STRERROR && !defined(strerror)
char *strerror (int errnum);
#endif

#endif /* COMMON_H */
