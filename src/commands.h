/* 
   cadaver, command-line DAV client
   Copyright (C) 1999-2001, Joe Orton <joe@orton.demon.co.uk>
                                                                     
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

#ifndef COMMANDS_H
#define COMMANDS_H

#include "cadaver.h"

extern int child_running; /* true when we have a child running */

#define CMD_VARY 9999

/* Returns the command structure for the command of given name. */
const struct command *get_command(const char *name);

/* Returns absolute path which is 'filename' relative to 'path' 
 * (which must already be an absolute path). e.g.
 *    resolve_path("/dav/", "asda") == "/dav/asda"
 *    resolve_path("/dav/", "/asda") == "/asda"
 * Also removes '..' segments, e.g.
 *    resolve_path("/dav/foobar/", "../fish") == "/dav/fish"
 * If isdir is true, ensures the return value has a trailing slash.
 */
char *resolve_path(const char *dir, const char *filename, int isdir);

/* Displays cadaver version details. */
void execute_about(void);

/* Returns owner href. */
char *getowner(void);

void out_success(void);
void out_start(const char *verb, const char *noun);
void out_result(int ret);
int out_handle(int ret);

#endif /* COMMANDS_H */
