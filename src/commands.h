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

/* Naming conventions:
 *
 * "native path" -> string in native character encoding
 * "URI path" -> absolute URI path segment (escaped UTF-8 string)
 */

/* Convert a URI path to a native path. */
char *native_path_from_uri(const char *uri_path);

/* Convert a relative native path into a URI path, resolved against
 * the session URI path, e.g. "../fish food.txt" ->
 * "/dav/fish%20food.txt" */
char *uri_resolve_native(const char *native);

/* Convert a relative native path into a URI path with a trailing
 * slash. */
char *uri_resolve_native_coll(const char *native);

/* Displays cadaver version details. */
void execute_about(void);

/* Returns owner href. */
char *getowner(void);

/* Output charset if using iconv(). */
extern const char *out_charset;

void out_success(void);
void out_start(const char *verb, const char *noun);

/* Start a command, using a URI-path noun argument. */
void out_start_uri(const char *verb, const char *uri_path);
void out_result(int ret);
int out_handle(int ret);

#endif /* COMMANDS_H */
