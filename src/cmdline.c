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

#include "config.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>

#include <ne_string.h>
#include <ne_alloc.h>

#include "i18n.h"
#include "glob.h"
#include "basename.h"

#include "common.h"
#include "commands.h"
#include "cadaver.h"
#include "cmdline.h"
#include "utils.h"

static int has_glob_pattern(const char *str) {
    const char *pnt;
    for (pnt = str; *pnt != '\0'; pnt++)
	if (*pnt=='*' || *pnt=='[' || *pnt=='?')
	    return 1;
    return 0;
}

/* Gets the next token for parse_command...
 * Starts at position in line.
 * DFA states:
 *   0: chewing leading whitespace
 *   1: chewing characters, normally
 *   2: chewing characters in a quote
 *   3: just got a backslash in normal chew
 *   4: just got a backslash in a quoted chew
 *   8: ignoring comment
 *   9: acceptance state
 * 
 * State diagram is left as an exercise to the reader, since 
 * I'm not going to draw it in ASCII. ;)
 *
 * Returns the token, malloc()-allocated, or NULL on end-of-line.
 * Position updated to point after token.
 */
static char *gettoken(const char *line, const char **pointer) 
{
    const char *pnt;
    int state = 0, pos = 0;
    char buf[BUFSIZ], quote = 0; /* = 0 to keep gcc -Wall happy */
    
    pnt = *pointer;
    
#define ISQUOTE(x) (x=='\'' || x=='\"')
#define ISWHITE(x) (x==' ' || x=='\t')

     while (*pnt != '\0' && state != 9 && pos < BUFSIZ) {
	 switch (state) {
	 case 0: /* leading whitespace chew */
	     if (ISQUOTE(*pnt)) {
		 state = 2;
		 quote = *pnt;
	     } else if (*pnt == '#') {
		 state = 8;
	     } else if (!ISWHITE(*pnt)) {
		 buf[pos++] = *pnt;
		 state = 1;
	     }
	     break;
	 case 1: /* normal chew */
	     if (ISWHITE(*pnt)) {
		 state = 9;
	     } else if (*pnt == '#') {
		 state = 8;
	     } else if (*pnt == '\\') {
		 state = 3;
	     } else {
		 buf[pos++] = *pnt;
	     }
	     break;
	 case 2: /* quoted chew */
	     if (*pnt == quote) {
		 state = 9;
	     } else if (*pnt == '\\') {
                state = 4;
	     } else {
		 buf[pos++] = *pnt;
	     }
	     break;
	 case 3: /* chew an escaped literal */
	     buf[pos++] = *pnt;
	     state = 1;
	     break;
	 case 4: /* backslash in quoted chew... like 3 except
		  * we switch back to state 2 afterwards */
	     buf[pos++] = *pnt;
	     state = 2;
	     break;
	 case 5: /* comment chew */
	     break;
	 }
	 pnt++;
     }
     
#undef ISQUOTE
#undef ISWHITE

     /* overflowed the buffer */
     if (pos == BUFSIZ) return NULL;
     
     buf[pos] = '\0';
     *pointer = pnt;
     if (pos > 0) {
#ifdef I_AM_A_LUMBERJACK
	 /* a little hack; does env. var expansion...
	  * 1) is this really useful?
	  * 2) this should be done in parse_command not gettoken
	  */
	 if (buf[0] == '$') {
	     char *val = getenv(&buf[1]);
	     if (val) return ne_strdup(val);
	 }
#endif
	 return ne_strdup(buf);
     } else {
	 return NULL;
     }
}

/* This is about as efficient as painting a wall with a toothbrush.
 * And it will go just as fast, if you have a slow link to your
 * server. 
 */

/* 
   Okay, and this was a real swine to get working.
   The glob() in glibc-2.1.2 (I haven't checked other versions)
   has a REAL_DIR_ENTRY() macro which checks whether the d_ino
   field is set. If it isn't, the entry is IGNORED! 
   
   Actually, the glob() in glib-2.1.2 is broken, so we supply
   a fixed version in lib/.
*/

volatile int interrupt_state; /* for glob */

struct dg_ctx {
    int rootlen;
    struct resource *files;
    struct resource *current;
};

static void *davglob_opendir(const char *dir) 
{
    struct dg_ctx *ctx = NULL;
    struct resource *files;
    char *real_path = resolve_path(session.uri.path, dir, 1);
    NE_DEBUG(DEBUG_FILES, "opendir: %s\n", dir);
    switch (fetch_resource_list(session.sess, real_path, 1, 0, &files)) {
    case NE_OK:
	ctx = ne_calloc(sizeof *ctx);
	ctx->rootlen = strlen(real_path);
	ctx->files = files;
	ctx->current = ctx->files;
	break;
    case NE_AUTH:
	errno = EACCES;
	break;
    default:
	/* Let them know it doesn't exist. */
	errno = ENOENT;
	break;
    }	
    free(real_path);
    return (void *)ctx;
}

/* Mocks up a dummy dirent structure */
static struct dirent *davglob_readdir(void *dir)
{
    static struct dirent ent;
    struct dg_ctx *ctx = dir;
    NE_DEBUG(DEBUG_FILES, "readdir:");
    if (!ctx->current) { 
	NE_DEBUG(DEBUG_FILES, "none.\n");
	return NULL;
    } else {
	memset(&ent, 0, sizeof(struct dirent));
	ne_strnzcpy(ent.d_name, ctx->current->uri + ctx->rootlen, 255);
	/* FIXME: non-portable, there's an autoconf 
	 * test for this, "d-ino.m4" */
	ent.d_ino = 1;
	NE_DEBUG(DEBUG_FILES, "%s\n", ent.d_name);
	ctx->current = ctx->current->next;
	return &ent;
    }
}

static void davglob_closedir(void *dir) 
{
    struct dg_ctx *ctx = dir;
    NE_DEBUG(DEBUG_FILES, "closedir\n");
    free_resource_list(ctx->files);
    free(ctx);
}

static int davglob_stat(const char *filename, struct stat *st) {
    /* presumption: all glob needs to know is whether it's a directory
     * or not. I think this is true for the glob in glibc2 */
    char *dir;
    NE_DEBUG(DEBUG_FILES, "stat %s\n", filename);
    dir = resolve_path(session.uri.path, filename, 1);
    if (getrestype(dir) == resr_collection) {
	st->st_mode = S_IFDIR;
    } else {
	st->st_mode = S_IFREG;
    }
    free(dir);
    return 0;
}

static void davglob_interrupt(int sig) {
    interrupt_state = 1;
}

static int davglob_errfunc(const char *filename, int errcode) 
{
    output(o_finish, "Error on %s: %s\n", filename, strerror(errcode));
    return 0;
}

char **parse_command(const char *line, int *count) 
{ 
    char *token, **tokens = NULL;
    const char *pnt = line;
    int numtokens = 0, matches = 0;
    const struct command *cmd = NULL;

#define ADDTOK(x) 						\
do {								\
    tokens = realloc(tokens, ++numtokens*sizeof(char *));	\
    tokens[numtokens-1] = x;					\
} while (0)

    while ((token = gettoken(line, &pnt)) != NULL) {
	if (!numtokens) {
	    /* The first token: get the command */
	    cmd = get_command(token);
	    ADDTOK(token);
	} else if (has_glob_pattern(token) && cmd &&
		   ((cmd->scope == parmscope_remote && session.connected) || 
		     (cmd->scope == parmscope_local))) {
	    /* Let us Glob */
	    glob_t gl = {0};
	    int ret, flags = GLOB_NOSORT;
	    void (*oldhand)(int sig);
	    
	    output(o_start, _("[Matching..."));
	    interrupt_state = 0;
	    
	    /* The glob() we have compiled here allows us to set a
	     * global variable 'interrupt_state' to 1 when the user
	     * wants to interrupt the glob expansion (particularly
	     * relevant for long slow remote globs)... here's the 
	     * handler */
	    oldhand = signal(SIGINT, davglob_interrupt);
	    
	    /* This is nice. We expand the glob in the same way
	     * whether it is a local or a remote glob, except we lob
	     * in the remote-glob handlers here if the command
	     * requires remote globs to be expanded */
	    if (cmd->scope == parmscope_remote) {
		gl.gl_closedir = davglob_closedir;
		gl.gl_opendir = davglob_opendir;
		gl.gl_readdir = davglob_readdir;
		gl.gl_stat = gl.gl_lstat = davglob_stat;
		flags |= GLOB_ALTDIRFUNC;
	    }
	    /* Do the Glob Thang */
	    ret = glob(token, flags, davglob_errfunc, &gl);
	    switch (ret) {
	    case 0: {
		unsigned int n;
		if (gl.gl_pathc > 1) {
		    output(o_finish, _("%ld matches.]\n"), (long)gl.gl_pathc);
		} else {
		    output(o_finish, _("1 match.]\n"));
		}
		for (n = 0; n < gl.gl_pathc; n++) {
		    /* Remote glob expanded to the escaped URIs, so we
		       need to unescape them. Local glob needs to be only
		       copied. */
		    if (cmd->scope == parmscope_remote)
			ADDTOK(ne_path_unescape(gl.gl_pathv[n]));
		    else
			ADDTOK(ne_strdup(gl.gl_pathv[n]));
		}
		matches++;
	    } break;
	    case GLOB_NOSPACE:
		output(o_finish, "failed: out of memory.]\n");
		break;
	    case GLOB_ABORTED:
		output(o_finish, "aborted]\n");
		break;
	    case GLOB_NOMATCH:
		output(o_finish, "no matches.]\n");
		break;
	    default:
		output(o_finish, "failed.]\n");
	    }
	    if (ret) {
		/* For all the failure cases, put in the glob instead.
		 * Perhaps we should fail here instead... but, this is
		 * what bash does, so we stick with consistent
		 * behaviour. TODO: this could be an option.
		 */
		ADDTOK(token);
	    } else {
		/* Otherwise, we don't use the actual token */
		free(token);
	    }
	    globfree(&gl);
	    signal(SIGINT, oldhand);
	} else {
	    ADDTOK(token);
	}
    }

    *count = numtokens;
    /* add a NULL at the end of the list */
    ADDTOK(NULL);

    return tokens;
}
