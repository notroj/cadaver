/* 
   cadaver, command-line DAV client
   Copyright (C) 1999-2001, Joe Orton <joe@manyfish.co.uk>
                                                                     
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

/* Options handling */

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>

#include <ne_request.h>
#include <ne_utils.h>
#include <ne_alloc.h>
#include <ne_basic.h> /* NE_DEPTH_* */

#include "common.h"
#include "cadaver.h"
#include "options.h"

static void set_debug(const char *new);
static void unset_debug(const char *new);
static void disp_debug(void);

static void set_lockscope(const char *new);
static void unset_lockscope(const char *new);
static void disp_lockscope(void);

static void set_lockdepth(const char *new);
static void unset_lockdepth(const char *new);
static void disp_lockdepth(void);

static void set_searchdepth(const char *new);
static void unset_searchdepth(const char *new);
static void disp_searchdepth(void);

/* Option holders */

static int enable_expect, presume_utf8, overwrite, quiet, searchall;

enum ne_lock_scope lockscope;
int lockdepth;

/* search option global */
int searchdepth;

static struct option {
    const char *name;
    enum option_id id;
    void *holder; /* for bool + string options */
    enum {
	opt_bool,
	opt_string,
	opt_handled
    } type;
    /* for handled options */
    void (*set)(const char *);
    void (*unset)(const char *);
    void (*display)(void);
    /* for all options */
    const char *help;
    /* for handled options */
    const char *handle_help;
} options[] = {
#define B(x,v,h) { #x, opt_##x, v, opt_bool, NULL, NULL, NULL, h, NULL }
    /* Booleans */
    B(tolerant, &tolerant, "Tolerate non-WebDAV collections"),
    B(overwrite, &overwrite, "Enable overwrite (e.g. on copy/move operations"),
    B(expect100, &enable_expect, "Enable use of 'Expect: 100-continue' header"),
    B(utf8, &presume_utf8, "Presume filenames etc are UTF-8 encoded"),
    B(quiet, &quiet, "Whether to display connection status messages"),
    B(searchall, &searchall, "Whether to search and display all props including dead props"),
#undef B
#define S2(x,y, h) { x, y, NULL, opt_string, NULL, NULL, NULL, h, NULL }
#define S(x,h) S2(#x, opt_##x, h)
    S(lockowner, "Lock owner URI"),
    S(lockstore, "Persistent lock storage file"),
    S(editor, "Editor to use with `edit' command"),
    S2("client-cert", opt_clicert,
       "Client certificate to use for SSL connections."),
    S(namespace, "Namespace to use for propset/propget commands."),
    S(pager, "Command to run for less/more commands."),
    S(proxy, "Hostname of proxy server"),
    S(searchorder,  "Search ascending props options"),
    S(searchdorder, "Search descending props options"),
    { "proxy-port", opt_proxy_port, NULL, opt_string, NULL, NULL, NULL,
      "Port to use on proxy server", NULL },
#undef S
    { "debug", opt_debug, NULL, opt_handled,
      set_debug, unset_debug, disp_debug, "Debugging options",
      "The debug value is a list of comma-separated keywords.\n"
      "Valid keywords are: socket, http, xml, httpauth, cleartext."
    },
    { "lockscope", opt_lockscope, NULL, opt_handled,
      set_lockscope, unset_lockscope, disp_lockscope, "Lock scope options",
      "The lockscope value must be one of two valid keywords: exclusive or shared."
    },
    { "lockdepth", opt_lockdepth, NULL, opt_handled,
      set_lockdepth, unset_lockdepth, disp_lockdepth, "Lock depth options",
      "The lockdepth value must be 0 or infinity."
    },

    /* Several options for search */
    { "searchdepth", opt_searchdepth, NULL, opt_handled,
      set_searchdepth, unset_searchdepth, disp_searchdepth, "Search depth options",
      "The searchdepth value must be 0, 1 or infinity."
    },

    { NULL, 0 }
};

static const struct {
    const char *name;
    int val;
} debug_map[] = {
    { "xml", NE_DBG_XML },
    { "xmlparse", NE_DBG_XMLPARSE },
    { "http", NE_DBG_HTTP },
    { "socket", NE_DBG_SOCKET },
    { "ssl", NE_DBG_SSL },
    { "httpauth", NE_DBG_HTTPAUTH },
    { "httpbody", NE_DBG_HTTPBODY },
    { "cleartext", NE_DBG_HTTPPLAIN },
    { "files", DEBUG_FILES },
    { "locks", NE_DBG_LOCKS },
    { NULL, 0 }
};

static void display_options(void) 
{
    int n;
    printf("Options:\n");
    for (n = 0; options[n].name != NULL; n++) {
	int *val = (int *)options[n].holder;
	switch (options[n].type) {
	case opt_bool:
	    printf(" %15s: %s\n", options[n].name, *val?"on":"off");
	    break;
	case opt_string:
	    if (options[n].holder == NULL) {
		printf(" %15s: unset\n", options[n].name);
	    } else {
		printf(" %15s: %s\n", options[n].name, 
		       (char *)options[n].holder);
	    }
	    break;
	case opt_handled:
	    printf(" %15s: ", options[n].name);
	    (*options[n].display)();
	    printf("\n");
	    break;
	}
    }
}

static void do_debug(const char *set, int setit)
{
    char *opts, *pnt;

    if (!setit && !set) {
	ne_debug_mask = 0;
	return;
    }
    
    pnt = opts = ne_strdup(set);

    do {
	int d, got = 0;
	char *opt = ne_token(&pnt, ',');

	for (d = 0; debug_map[d].name != NULL; d++) {
	    if (strcasecmp(opt, debug_map[d].name) == 0) {
		if (setit) {
		    ne_debug_mask |= debug_map[d].val ;
		} else {
		    ne_debug_mask &= ~debug_map[d].val;
		}
		got = 1;
	    }
	}

	if (!got) {
	    printf("Debug option %s unknown.\n", opt);
	}
    } while (pnt != NULL);
    
    free(opts);
}

static void set_debug(const char *set)
{
    do_debug(set, 1);
}

static void unset_debug(const char *s)
{
    do_debug(s, 0);
}

static void disp_debug(void)
{
    int n, flag=0;
    putchar('{');
    for (n = 0; debug_map[n].name != NULL; n++) {
	if (ne_debug_mask & debug_map[n].val) {
	    printf("%s%s", flag++?",":"", debug_map[n].name);
	}
    }
    putchar('}');
}

static void set_lockscope(const char *set)
{
    if (strcasecmp(set,"exclusive") == 0)
	lockscope = ne_lockscope_exclusive;
    else if (strcasecmp(set,"shared") == 0)
	lockscope = ne_lockscope_shared;
    else
	printf("Invalid value for lockscope. Try `set lockscope' for more info.\n");
}

static void unset_lockscope(const char *s)
{
    lockscope = ne_lockscope_exclusive;
}

static void disp_lockscope(void)
{
    if (lockscope == ne_lockscope_exclusive)
	printf("exclusive");
    else if (lockscope == ne_lockscope_shared)
	printf("shared");
    else
	printf("illegal value");
}

static void set_lockdepth(const char *set)
{
    if (strcmp(set, "0") == 0 ||
	strcasecmp(set, "zero") == 0)
	lockdepth = NE_DEPTH_ZERO;
    else if (strcasecmp(set, "infinite") == 0 ||
	     strcasecmp(set, "infinity") == 0)
	lockdepth = NE_DEPTH_INFINITE;
    else
	printf("Invalid value for lockdepth. Try `set lockdepth' for more info.\n");
}

static void unset_lockdepth(const char *s)
{
    lockdepth = NE_DEPTH_INFINITE;
}

static void disp_lockdepth(void)
{
    if (lockdepth == NE_DEPTH_ZERO)
	printf("zero");
    else if (lockdepth == NE_DEPTH_INFINITE)
	printf("infinite");
    else
	printf("illegal value");
}


static void set_searchdepth(const char *set)
{
    if (strcmp(set, "0") == 0 ||
	strcasecmp(set, "zero") == 0)
	searchdepth = NE_DEPTH_ZERO;
    else if (strcasecmp(set, "1") == 0 ||
	     strcasecmp(set, "one") == 0)
	searchdepth = NE_DEPTH_ONE;
    else
	searchdepth = NE_DEPTH_INFINITE;
}

static void unset_searchdepth(const char *s)
{
    searchdepth = NE_DEPTH_INFINITE;
}

static void disp_searchdepth(void)
{
    if (searchdepth == NE_DEPTH_ZERO)
	printf("0");
    else if (searchdepth == NE_DEPTH_ONE)
	printf("1");
    else
	printf("infinity");
}

static const struct option *find_option(const char *name)
{
    int n;
    
    for (n = 0; options[n].name != NULL; n++)
        if (strcasecmp(options[n].name, name) == 0)
            return &options[n];
    
    return NULL;
}

void execute_set(const char *opt, const char *newv)
{
    if (opt == NULL) {
	display_options();
    } else {
	int n;
	for (n = 0; options[n].name != NULL; n++) {
	    if (strcasecmp(options[n].name, opt) == 0) {
		switch (options[n].type) {
		case opt_bool:
		    if (newv) {
			printf("%s is a boolean option.\n", opt);
		    } else {
			*(int *)options[n].holder = 1;
		    }
		    break;
		case opt_string:
		    if (newv == NULL) {
			printf("You must give a new value for %s\n", opt);
		    } else {
			char *val = options[n].holder;
			if (val != NULL) {
			    free(val);
			}
			options[n].holder = ne_strdup(newv);
		    }
		    break;
		case opt_handled:
		    if (!newv) {
			printf("%s must be given a value:\n%s\n", opt,
				options[n].handle_help);
		    } else {
			(*options[n].set)(newv);
		    }
		    break;
		}
		return;
	    }
	}
	printf("Unknown option: %s.\n", opt);
    }
}

void execute_unset(const char *opt, const char *newv)
{
    int n;
    for (n = 0; options[n].name != NULL; n++) {
	if (strcasecmp(options[n].name, opt) == 0) {
	    switch (options[n].type) {
	    case opt_bool:
		if (newv != NULL) {
		    printf("%s ia a boolean option.\n", opt);
		} else {
		    *(int *)options[n].holder = 0;
		}
		break;
	    case opt_string:
		/* FIXME: This is bad UI */
		if (newv != NULL) {
		    printf("%s cannot take a value to unset.\n", opt);
		} else {
		    char *v = options[n].holder;
		    free(v);
		    options[n].holder = NULL;
		}
		break;	       
	    case opt_handled:
		(*options[n].unset)(newv);
		break;
	    }
	    return;
	}
    }
    printf("Unknown option: %s.\n", opt);
}

void execute_describe(const char *name)
{
    const struct option *opt = find_option(name);

    if (opt == NULL) {
        printf("Option `%s' not known.\n", name);
        return;
    }

    printf("Option `%s': %s\n", opt->name, opt->help);
    if (opt->handle_help) puts(opt->handle_help);
}

void *get_option(enum option_id id)
{
    int n;
    for (n = 0; options[n].name != NULL; n++) {
	if (options[n].id == id) {
	    return options[n].holder;
	}
    }
    return NULL;
}

int get_bool_option(enum option_id id)
{
    int *ret = get_option(id);
    if (ret == NULL)
	return 0;
    return *ret;
}

void set_bool_option(enum option_id id, int truth)
{
    int *opt = get_option(id);
    if (opt != NULL)
	*opt = truth;
}

void set_option(enum option_id id, void *newval)
{
    int n;
    for (n = 0; options[n].name != NULL; n++) {
	if (options[n].id == id) {
	    options[n].holder = newval;
	    return;
	}
    }
}
