/* 
   cadaver, command-line DAV client
   Copyright (C) 1999-2005, 2008, Joe Orton <joe@orton.demon.co.uk>
                                                                     
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

#ifndef CADAVER_H
#define CADAVER_H

#include <ne_session.h> /* for ne_session */
#include <ne_locks.h> /* for ne_lock_store */

#ifdef HAVE_LIBREADLINE

/* readline requires FILE *, silly thing */
#include <stdio.h>

#ifdef HAVE_READLINE_H
#include <readline.h>
#else /* !HAVE_READLINE_H */
#ifdef HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#endif
#endif /* HAVE_READLINE_H */

#ifdef HAVE_HISTORY_H
#include <history.h>
#else
#ifdef HAVE_READLINE_HISTORY_H
#include <readline/history.h>
#endif
#endif /* HAVE_HISTORY_H */

#endif

#include "common.h"

/* DON'T FORGET TO ADD A NEW COMMAND ALIAS WHEN YOU ADD A NEW COMMAND */
enum command_id {
    cmd_ls, cmd_cd, cmd_quit, cmd_open, cmd_close, cmd_about, cmd_pwd,
    cmd_help, cmd_put, cmd_get, cmd_mkcol, cmd_delete, cmd_move, cmd_copy, 
    cmd_less, cmd_cat, cmd_lpwd, cmd_lcd, cmd_lls, cmd_mput, cmd_mget,
    cmd_echo, cmd_set, cmd_unset, cmd_rmcol, cmd_lock, cmd_unlock,
    cmd_steal, cmd_discover, cmd_showlocks, cmd_propedit, cmd_propnames,
    cmd_propget, cmd_propset, cmd_propdel, cmd_chexec, cmd_edit, cmd_logout,
    cmd_describe, cmd_search, cmd_version, cmd_checkin, cmd_checkout,
    cmd_uncheckout, cmd_history, cmd_label,
    cmd_unknown
/* DON'T FORGET TO ADD A NEW COMMAND ALIAS WHEN YOU ADD A NEW COMMAND */
};

struct command {
    enum command_id id; /* unique ID */
    const char *name; /* real command name. */
    unsigned int needs_connection; /* true if only works when connected */
    int min_args, max_args; /* min and max # of arguments */
    enum { /* parameter scope */
	parmscope_none, /* not at all */
	parmscope_option, /* option parameter */
	parmscope_local, /* local filenames */
	parmscope_remote /* remote filename */
    } scope;
    union {
	void (*take0)(void);
	void (*take1)(const char *);
	void (*take2)(const char *, const char *);
	void (*take3)(const char *, const char *, const char *);
	void (*takeV)(int, const char **);
    } handler;
    const char *call; /* command usage */
    const char *short_help; /* single-line help message */
};

extern char *proxy_hostname;
extern int proxy_port;

struct session {
    ne_uri uri;
    ne_session *sess;
    int connected; /* non-zero when connected. */
    int isdav; /* non-zero if real DAV collection */
    ne_lock_store *locks; /* stored locks */
    char *lastwp; /* last working path. */
};

/* Current session state. */
extern struct session session;

/* Sets the current collection to the given path.  Returns zero on
 * success, non-zero if newpath is an untolerated non-WebDAV
 * collection. */
int set_path(const char *newpath);

extern int tolerant;

#ifdef HAVE_LIBREADLINE
char *command_generator(const char *text, int state);
#else
/* roll our own */
char *readline(const char *prompt);
#endif

enum resource_type {
    resr_normal = 0,
    resr_collection,
    resr_reference,
    resr_error
};

#ifdef HAVE_UNSIGNED_LONG_LONG
typedef unsigned long long dav_size_t;
#define FMT_DAV_SIZE_T "ll"
#ifdef HAVE_STRTOULL
#define DAV_STRTOL strtoull
#endif
#else
typedef unsigned long dav_size_t;
#define FMT_DAV_SIZE_T "l"
#endif

#ifndef DAV_STRTOL
#define DAV_STRTOL strtol
#endif

struct resource {
    char *uri;
    char *displayname;
    enum resource_type type;
    dav_size_t size;
    time_t modtime;
    int is_executable;
    int is_vcr;    /* Is version resource. 0: no vcr, 1 checkin 2 checkout */
    char *error_reason; /* error string returned for this resource */
    int error_status; /* error status returned for this resource */
    struct resource *next;
};

void close_connection(void);
void open_connection(const char *url);

void execute_ls(const char *remote);
void execute_edit(const char *remote);

/* Determine whether the resource is a version controlled resource 
 * 1: Checkin, 2: Checkout, 0: Otherwise */
int is_vcr(const char *remote);

void execute_version(const char *remote);
void execute_checkin(const char *remote);
void execute_checkout(const char *remote);
void execute_uncheckout(const char *remote);
void execute_history(const char *remote);

void execute_label(const char *res, const char *act, const char *value);

void execute_search(int count, const char **args);

void free_resource(struct resource *res);
void free_resource_list(struct resource *res);

int fetch_resource_list(ne_session *sess, const char *uri,
			int depth, int include_uri,
			struct resource **reslist);

/* Command feedback handling */
enum output_type {
    o_start,
    o_upload,
    o_download,
    o_finish
};

void output(enum output_type, const char *fmt, ...)
#ifdef __GNUC__
                __attribute__ ((format (printf, 2, 3)))
#endif /* __GNUC__ */
;

#endif /* CADAVER_H */

