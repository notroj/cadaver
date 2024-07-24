/* 
   cadaver, command-line DAV client
   Copyright (C) 1999-2024, Joe Orton <joe@manyfish.co.uk>,
   except where otherwise indicated.

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

/* Some UI guidelines:
 *  1. Use dispatch, or out_* to do UI. This makes it CONSISTENT.
 *  2. Get some feedback on the screen before making any requests
 *     to the server. Tell them what is going on: remember, on a slow
 *     link or a loaded server,5~ a request can take AGES to return.
 */

#include "config.h"

#include <sys/types.h>

#include <sys/stat.h>
#include <sys/wait.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <fcntl.h>
#include <errno.h>

/* readline requires FILE *, silly thing */
#include <stdio.h>

#ifdef HAVE_READLINE_H
#include <readline.h>
#elif HAVE_READLINE_READLINE_H
#include <readline/readline.h>
#endif

#ifdef NEED_SNPRINTF_H
#include "snprintf.h"
#endif

#include <ne_request.h>
#include <ne_basic.h>
#include <ne_auth.h> /* for http_forget_auth */
#include <ne_redirect.h>
#include <ne_props.h>
#include <ne_string.h>
#include <ne_uri.h>
#include <ne_locks.h>
#include <ne_alloc.h>
#include <ne_dates.h>

#include "i18n.h"
#include "basename.h"
#include "dirname.h"
#include "cadaver.h"
#include "commands.h"
#include "options.h"
#include "utils.h"

/* Local variables */
int child_running; /* true when we have a child running */

/* Command alias mappings */
const static struct {
    enum command_id id;
    const char *name;
} command_names[] = {
    /* The direct mappings */
#define C(n) { cmd_##n, #n }
    C(ls), C(cd), C(quit), C(open), C(logout), C(close), C(set), C(unset), 
    C(pwd), C(help), C(put), C(get), C(mkcol), C(delete), C(move), C(copy),
    C(less), C(cat), C(lpwd), C(lcd), C(lls), C(echo), C(quit), C(about),
    C(mget), C(mput), C(rmcol), C(lock), C(unlock), C(discover), C(steal),
    C(chexec), C(showlocks), C(version), C(propget), C(propset), C(propdel),
    C(describe), C(search),
    C(version), C(checkin), C(checkout), C(uncheckout), C(history),
    C(label), 
#if 0
C(propedit), 
#endif
C(propnames), C(edit),
#undef C
    /* And now the real aliases */
    { cmd_less, "more" }, { cmd_mkcol, "mkdir" }, 
    { cmd_delete, "rm" }, { cmd_copy, "cp"}, { cmd_move, "mv" }, 
    { cmd_help, "h" }, { cmd_help, "?" },
    { cmd_quit, "exit" }, { cmd_quit, "bye" },
    { cmd_unknown, NULL }
};    

extern const struct command commands[]; /* prototype */

/* Tell them we are doing 'VERB' to 'NOUN'.
 * (really 'METHOD' to 'RESOURCE' but hey.) */
void out_start(const char *verb, const char *noun)
{
    output(o_start, "%s `%s':", verb, noun);
}

void out_start_uri(const char *verb, const char *path)
{
    char *native_path = native_path_from_uri(path);
    output(o_start, "%s `%s':", verb, native_path);
    ne_free(native_path);
}

void out_success(void)
{
    output(o_finish, _("succeeded.\n"));
}

void out_result(int ret)
{
    switch (ret) {
    case NE_OK:
	out_success();
	break;
    case NE_AUTH:
    case NE_PROXYAUTH:
	output(o_finish, _("authentication failed.\n"));
	break;
    case NE_CONNECT:
	output(o_finish, _("could not connect to server.\n"));
	break;
    case NE_TIMEOUT:
	output(o_finish, _("connection timed out.\n"));
	break;
    default:
        if (ret == NE_REDIRECT) {
            const ne_uri *dest = ne_redirect_location(session.sess);
            if (dest) {
                char *uri = ne_uri_unparse(dest);
                output(o_finish, _("redirect to %s\n"), uri);
                ne_free(uri);
                break;
            }
        }
	output(o_finish, _("failed:\n%s\n"), ne_get_error(session.sess));
	break;
    }
}

int out_handle(int ret)
{
    out_result(ret);
    return (ret == NE_OK);
}

/* Convert native string to UTF-8, returns malloc-allocated. */
static char *utf8_from_native(const char *native)
{
    if (!get_bool_option(opt_utf8)) /* TODO */ abort();
    return ne_strdup(native);
}

/* Convert UTF-8 to native string, malloc-allocated. */
static char *native_from_utf8(const char *native)
{
    if (!get_bool_option(opt_utf8)) /* TODO */ abort();
    return ne_strdup(native);
}

/* The actual commands */
#ifdef HAVE_LIBREADLINE

/* Command name generator for readline.
 * Copied almost verbatim from the info doc */
char *command_generator(const char *text, int state)
{
    static int i, len;
    const char *name;

    if (!state) {
	i = 0;
	len = strlen(text);
    }

    while ((name = command_names[i].name) != NULL) {
	i++;
	if (strncmp(name, text, len) == 0) {
	    return ne_strdup(name);
	}
    }
    return NULL;
}

#endif

static void execute_logout(void)
{
    ne_forget_auth(session.sess);
}

const struct command *get_command(const char *name)
{
    int n, m;
    for(n = 0; command_names[n].name != NULL; n++) {
	if (strcasecmp(command_names[n].name, name) == 0) {
	    for(m = 0; commands[m].id != cmd_unknown; m++) {
		if (commands[m].id == command_names[n].id)
		    return &commands[m];
	    }
	    return NULL;
	}
    }
    return NULL;
}


static void dispatch(const char *verb, const char *filename, 
		     int (*func)(ne_session *, const char *), const char *arg)
{
    out_start(verb, filename);
    out_result((*func)(session.sess, arg));
}

char *getowner(void)
{
    char *ret, *owner = get_option(opt_lockowner);
    if (owner) {
	ret = ne_concat("<href>", owner, "</href>", NULL);
	return ret;
    } else {
	return NULL;
    }
}

/* Resolve path, appending trailing slash if resource is a
 * collection. If 'collection' is non-NULL, *collection is set to
 * non-zero iff the resource is a collection.
 * ### FIXME: this fails if "/dav/foo" redirects to "/dav/foo/" which
 * is a collection.
 */
static char *uri_resolve_native_true(const char *path, int *collection)
{
    char *uri_path = uri_resolve_native(path);
    int is_coll = getrestype(uri_path) == resr_collection;

    if (collection) *collection = is_coll;

    if (is_coll && !ne_path_has_trailing_slash(uri_path)) {
        char *tmp = ne_concat(uri_path, "/", NULL);
        ne_free(uri_path);
        uri_path = tmp;
    }

    return uri_path;
}

static const char *get_lockscope(enum ne_lock_scope s)
{
    switch (s) {
    case ne_lockscope_exclusive: return _("exclusive");
    case ne_lockscope_shared: return _("shared");
    default: return _("unknown");
    }
}

static const char *get_locktype(enum ne_lock_type t)
{
    if (t == ne_locktype_write) {
	return _("write");
    } else {
	return _("unknown");
    }
}

static const char *get_timeout(long t)
{
    static char buf[128];
    switch (t) {
    case NE_TIMEOUT_INFINITE: return _("infinite");
    case NE_TIMEOUT_INVALID: return _("invalid");
    default:
	sprintf(buf, _("%ld seconds"), t);
	return buf;
    }
}

static const char *get_depth(int d)
{
    switch (d) {
    case NE_DEPTH_INFINITE:
	return _("infinity");
    case 0:
	/* TODO: errr... do I need to i18n'ize numeric strings??? */
	return "0";
    case 1:
	return "1";
    default:
	return _("invalid");
    }
}

static void print_uri(const ne_uri *uri)
{
    char *str = ne_uri_unparse(uri);
    printf("%s", str);
    free(str);
}

static void print_lock(const struct ne_lock *lock)
{
    char *uri = ne_uri_unparse(&lock->uri);
    printf(_("Lock token <%s>:\n"
	     "  Depth %s on `%s'\n"
	     "  Scope: %s  Type: %s  Timeout: %s\n"
	     "  Owner: %s\n"), 
	   lock->token, get_depth(lock->depth), uri,
	   get_lockscope(lock->scope),
	   get_locktype(lock->type), get_timeout(lock->timeout),
	   lock->owner?lock->owner:_("(none)"));
    free(uri);
}

static void discover_result(void *userdata, const struct ne_lock *lock,
                            const ne_uri *uri, 
                            const ne_status *status)
{
    int *count = userdata;
    if (lock) {
	if (*count == 0) {
	    printf("\n");
	}
	print_lock(lock);
	*count += 1;
    } else {
	printf(_("Failed on %s: %d %s\n"), uri->path,
	       status->code, status->reason_phrase);
    }
}

static void steal_result(void *userdata, const struct ne_lock *lock, 
			 const ne_uri *uri, 
                         const ne_status *status)
{
    int *count = userdata;
    if (lock != NULL) {
	if (*count == 0) {
	    printf("\n");
	}
	print_uri(&lock->uri);
	printf(": <%s>\n", lock->token);
	ne_lockstore_add(session.locks, ne_lock_copy(lock));
	*count += 1;
    } else {
	printf(_("Failed on %s: %d %s\n"), uri->path,
	       status->code, status->reason_phrase);
    }
}

static void do_discover(const char *path, const char *mesg,
			ne_lock_result result_cb)
{
    char *uri_path = uri_resolve_native(path);
    int ret, count = 0;
    out_start(mesg, path);
    ret = ne_lock_discover(session.sess, uri_path, result_cb, &count);
    switch (ret) {
    case NE_OK:
	if (count == 0) {
	    printf(" no locks found.\n");
	}
	break;
    default:
	out_result(ret);
	break;
    }
    ne_free(uri_path);
}

static void execute_discover(const char *path)
{
    do_discover(path, _("Discovering locks on"), discover_result);
}

static void execute_steal(const char *path)
{
    do_discover(path, _("Stealing locks on"), steal_result);
}

static void execute_showlocks(void)
{
    int count = 0;
    struct ne_lock *lk;
    
    for (lk = ne_lockstore_first(session.locks); lk != NULL;
	 lk = ne_lockstore_next(session.locks), count++) {
        print_lock(lk);
    }

    if (count == 0) {
	printf(_("No owned locks.\n"));
    }
}

static void execute_lock(const char *path)
{
    char *uri_path;
    struct ne_lock *lock;
    int iscoll;

    uri_path = uri_resolve_native_true(path, &iscoll);
    if (iscoll)
        out_start(_("Locking collection"), path);
    else
	out_start(_("Locking"), path);

    lock = ne_lock_create();
    lock->scope = lockscope;
    lock->owner = getowner();
    lock->depth = iscoll ? lockdepth : NE_DEPTH_ZERO;
    ne_fill_server_uri(session.sess, &lock->uri);
    lock->uri.path = uri_path;

    if (out_handle(ne_lock(session.sess, lock))) {
	/* success: remember the lock. */
	ne_lockstore_add(session.locks, lock);
    }
    else {
	/* Otherwise, throw it away */
	ne_lock_destroy(lock);
    }
}

static void execute_unlock(const char *res)
{
    struct ne_lock *lock;
    ne_uri uri = session.uri; /* shallow copy */

    uri.path = uri_resolve_native_true(res, NULL);

    out_start(_("Unlocking"), res);
    lock = ne_lockstore_findbyuri(session.locks, &uri);
    if (!lock) {
	lock = ne_lock_create();
	lock->token = readline(_("Enter locktoken: "));
	if (!lock->token || strlen(lock->token) == 0) {
	    goto unlock_fail;
	}
	ne_fill_server_uri(session.sess, &lock->uri);
	lock->uri.path = ne_strdup(uri.path);
    } 
    else {
	/* remove the lock from the lockstore */
	ne_lockstore_remove(session.locks, lock);
    }

    out_result(ne_unlock(session.sess, lock));

unlock_fail:
    ne_free(uri.path);
    ne_lock_destroy(lock);
}

static void execute_mkcol(const char *path)
{
    char *uri_path = uri_resolve_native_coll(path);
    dispatch(_("Creating"), path, ne_mkcol, uri_path);
    ne_free(uri_path);
}

static int all_iterator(void *userdata, const ne_propname *pname,
			 const char *value, const ne_status *status)
{
    char *nnspace = native_from_utf8(pname->nspace);
    char *nname = native_from_utf8(pname->name);
    if (value != NULL) {
	char *nval = native_from_utf8(value);
        char *sval = ne_shave(nval, " \r\n\t");
	printf(_("- %s%s = %s\n"), nnspace, nname, sval);
	ne_free(nval);
    }
    else if (status) {
	printf(_("-- failed for %s%s: %s\n"), nnspace, nname, status->reason_phrase);
    }
    ne_free(nnspace);
    ne_free(nname);
    return 0;
}

static void pget_results(void *userdata, const ne_uri *uri, 
			 const ne_prop_result_set *set)
{
    ne_propname *pname = userdata;
    const char *value;
    char *nname;

    printf("\n");

    if (userdata == NULL) {
	/* allprop */
	ne_propset_iterate(set, all_iterator, NULL);
	return;
    }
    
    nname = native_from_utf8(pname->name);

    value = ne_propset_value(set, pname);
    if (value != NULL) {
	char *nval = native_from_utf8(value);
	printf(_("Value of %s is: %s\n"), nname, nval);
	ne_free(nval);
    }
    else {
	const ne_status *status = ne_propset_status(set, pname);
	
	if (status) {
	    printf(_("Could not fetch property: %d %s\n"), 
		   status->code, status->reason_phrase);
	} else {
	    printf(_("Server did not return result for %s\n"),
		   nname);
	}
    } 

    ne_free(nname);
}

/* Change property 'name' on 'uri': if value is non-NULL, set property
 * to have new value, else delete it. */
static void propop(const char *descr, const char *path,
		   const char *name, const char *value)
{
    ne_proppatch_operation ops[2];
    ne_propname pname;
    char *uri_path = uri_resolve_native(path);
    char *val_utf = NULL, *name_utf = utf8_from_native(name);

    ops[0].name = &pname;
    if (value) {
	ops[0].type = ne_propset;
	ops[0].value = val_utf = utf8_from_native(value);
    }
    else {
	ops[0].type = ne_propremove;
    }
    ops[1].name = NULL;
    
    pname.name = name_utf;
    pname.nspace = get_option(opt_namespace);

    out_start(descr, path);
    out_handle(ne_proppatch(session.sess, uri_path, ops));

    if (val_utf) ne_free(val_utf);
    ne_free(name_utf);
    ne_free(uri_path);
}    


static void execute_propset(const char *res, const char *name, const char *value)
{
    propop(_("Setting property on"), res, name, value);
}

static void execute_propdel(const char *res, const char *name)
{
    propop(_("Deleting property on"), res, name, NULL);
}

static void execute_propget(const char *res, const char *name)
{
    ne_propname pnames[2] = {{NULL}, {NULL}};
    char *uri_path = uri_resolve_native(res), *uname = NULL;
    ne_propname *props;
    int ret;
    
    if (name == NULL) {
	props = NULL;
    } else {
	pnames[0].nspace = (const char *)get_option(opt_namespace);
	pnames[0].name = uname = utf8_from_native(name);
	props = pnames;
    }

    out_start(_("Fetching properties for"), res);
    ret = ne_simple_propfind(session.sess, uri_path, NE_DEPTH_ZERO, props,
                             pget_results, props);

    if (ret != NE_OK) {
	out_result(ret);
    }

    if (uname) ne_free(uname);
    ne_free(uri_path);
}

static int propname_iterator(void *userdata, const ne_propname *pname,
			     const char *value, const ne_status *st)
{
    printf("\n %s%s", pname->nspace, pname->name);
    return 0;
}

static void propname_results(void *userdata, const ne_uri *uri, 
			     const ne_prop_result_set *pset)
{
    ne_propset_iterate(pset, propname_iterator, NULL);
}

static void execute_propnames(const char *path)
{
    char *uri_path = uri_resolve_native(path);
    int ret;
    printf(_("Fetching property names for %s:"), path);
    if ((ret = ne_propnames(session.sess, uri_path, NE_DEPTH_ZERO,
                            propname_results, NULL)) != NE_OK) {
        out_result(ret);
    }
    else {
        putchar('\n');
    }
    ne_free(uri_path);
}

static void remove_locks(const char *p, int depth)
{
    struct ne_lock *lk;
    ne_uri sought;
    
    memset(&sought, 0, sizeof(sought));
    ne_fill_server_uri(session.sess, &sought);
    sought.path = ne_strdup(p);

    do {
	lk = ne_lockstore_findbyuri(session.locks, &sought);
	if (lk) {
	    ne_lockstore_remove(session.locks, lk);
	}
    } while (lk);

    ne_uri_free(&sought);
}

static void execute_delete(const char *path)
{
    char *uri_path = uri_resolve_native(path);
    out_start(_("Deleting"), path);
    if (getrestype(uri_path) == resr_collection) {
	output(o_finish, 
_("is a collection resource.\n"
"The `rm' command cannot be used to delete a collection.\n"
"Use `rmcol %s' to delete this collection and ALL its contents.\n"), 
               path);
    }
    else {
	if (out_handle(ne_delete(session.sess, uri_path))) {
	    remove_locks(uri_path, 0);
	}
    }
    ne_free(uri_path);
}

static void execute_rmcol(const char *path)
{
    char *uri_path = uri_resolve_native(path);
    out_start(_("Deleting collection"), path);
    if (getrestype(uri_path) != resr_collection) {
	output(o_finish, 
	       _("is not a collection.\n"
		 "The `rmcol' command can only be used to delete collections.\n"
		 "Use `rm %s' to delete this resource.\n"), path);
    }
    else {
	if (out_handle(ne_delete(session.sess, uri_path))) {
            remove_locks(uri_path, NE_DEPTH_INFINITE);
        }
    }
    ne_free(uri_path);
}

/*
 * --- Path conversion ---
 *
 * Native paths (input from the command-line) are unescaped, relative
 * references (potentially in a non-UTF-8 charset though this is not
 * handled yet).
 *
 * These must be converted to an 'abs_path' in the RFC 3986 grammar,
 * called a 'URI path' for convenience throughout here.
 */

/* Converts an input path (an unescaped, native charset, relative
 * path) to a URI path. */
static char *path_native_resolver(const char *path, int collection)
{
    ne_uri relative, result;
    char *tmp;

    if (!get_bool_option(opt_utf8)) {
        /* TODO */
        abort();
    }

    memset(&relative, 0, sizeof relative);
    relative.path = ne_path_escape(path);

    if (collection && !ne_path_has_trailing_slash(relative.path)) {
        tmp = ne_concat(relative.path, "/", NULL);
        ne_free(relative.path);
        relative.path = tmp;
    }

    ne_uri_resolve(&session.uri, &relative, &result);

    ne_uri_free(&relative);
    tmp = result.path;
    result.path = NULL;
    ne_uri_free(&result);

    return tmp;
}

/* Converts a native path to a URI path. */
char *uri_resolve_native(const char *native)
{
    return path_native_resolver(native, 0);
}

/* Converts a native path for a collection to a URI path, ensuring it
 * has a trailing slash. */
char *uri_resolve_native_coll(const char *native)
{
    return path_native_resolver(native, 1);
}

char *native_path_from_uri(const char *uri_path)
{
    if (!get_bool_option(opt_utf8)) {
        /* TODO */
        abort();
    }

    return ne_path_unescape(uri_path);
}

/* TODO: remove this. */
static char *escape_path(const char *p)
{
    return ne_path_escape(p);
}

/* TODO: remove this. */
/* Like resolve_path except more intelligent. 'p' must be
 * already URI-escaped; 'src' and 'dest' must not be. */
static char *clever_path(const char *p, const char *src, 
			 const char *dest)
{
    char *ret;
    int src_is_coll, dest_is_coll;
    dest_is_coll = (dest[strlen(dest)-1] == '/');
    src_is_coll = (src[strlen(src)-1] == '/');
    if (strcmp(dest, ".") == 0) {
	ret = resolve_path(p, base_name(src), false);
    } else if (strcmp(dest, "..") == 0) {
	char *parent;
	parent = ne_path_parent(p);
	ret = resolve_path(parent, base_name(src), false);
	free(parent);
    } else if (!src_is_coll && dest_is_coll) {
	/* Moving a file to a collection... the destination should
	 * be the basename of file concated with the collection. */
	char *tmp = resolve_path(p, dest, true);
        char *enc = escape_path(base_name(src));
	ret = ne_concat(tmp, enc, NULL);
        free(enc);
	free(tmp);
    } else {
	ret = resolve_path(p, dest, false);
    }
    return ret;
}

static const char *choose_pager(void)
{
    struct stat st;
    char *tmp;
    tmp = get_option(opt_pager);
    if (tmp != NULL)
	return tmp;
    /* TODO: add an autoconf for less/more? */
    tmp = getenv("PAGER");
    if (tmp != NULL) {
	return tmp;
    } else if (stat("/usr/bin/less", &st) == 0) {
	return "/usr/bin/less";
    } else if (stat("/bin/less", &st) == 0) {
	return "/bin/less";
    } else {
	return "/bin/more";
    }
}

static void execute_less(const char *native)
{
    const char *pager;
    char *uri_path = uri_resolve_native(native);
    FILE *p;
    int ret;

    pager = choose_pager();
    printf(_("Displaying `%s':\n"), native);

    p = popen(pager, "w");
    if (p == NULL) {
	printf(_("Error! Could not spawn pager `%s':\n%s\n"), pager,
		 strerror(errno));
        return;
    }

    child_running = true;
    ret = ne_get(session.sess, uri_path, fileno(p));
    if (ret) {
        pclose(p);
        out_result(ret);
    }
    else if ((ret = pclose(p)) != 0) {
        printf(_("Warning: Abnormal exit from pager (%d).\n"), ret);
    }
    child_running = false;
}

static void execute_cat(const char *native_path)
{
    char *uri_path = uri_resolve_native(native_path);
    int ret;

    ret = ne_get(session.sess, uri_path, STDOUT_FILENO);
    if (ret != NE_OK) {
        out_start(_("Fetching"), native_path);
        out_result(ret);
    }
}

static void do_copymove(int argc, const char *argv[],
			const char *v1, const char *v2,
			void (*cb)(const char *, const char *))
{
    /* We are guaranteed that argc > 2... */
    char *uri_dest;

    uri_dest = uri_resolve_native_coll(argv[argc-1]);
    if (getrestype(uri_dest) == resr_collection) {
	int n;
	char *real_src, *real_dest, *unescaped_dest;
	unescaped_dest = ne_path_unescape(uri_dest);
	for(n = 0; n < argc-1; n++) {
	    real_src = resolve_path(session.uri.path, argv[n], false);
	    real_dest = clever_path(session.uri.path, argv[n], unescaped_dest);
	    if (strcmp(real_src, real_dest) == 0) {
		char *unescaped_src, *unescaped_dst;
		unescaped_src = ne_path_unescape(real_src);
		unescaped_dst = ne_path_unescape(real_dest);
		printf(_("%s: %s and %s are the same resource.\n"), v2,
                       unescaped_src, unescaped_dest);
		ne_free(unescaped_dst);
		ne_free(unescaped_src);
	    } else {
		(*cb)(real_src, real_dest);
	    }
	    free(real_src);
	    free(real_dest);
	}
	ne_free(unescaped_dest);
    } else if (argc > 2) {
	printf(_("When %s multiple resources, the last "
                 "argument must be a collection.\n"), v1);
    } else {
	char *rsrc, *rdest;
	rsrc = resolve_path(session.uri.path, argv[0], false);
	rdest = resolve_path(session.uri.path, argv[1], false);
	/* Simple */
	(*cb) (rsrc, rdest);
	free(rsrc);
	free(rdest);
    }
    ne_free(uri_dest);
}

static void simple_move(const char *src, const char *dest) 
{
    char *unescaped_src, *unescaped_dest;
    unescaped_src = ne_path_unescape(src);
    unescaped_dest = ne_path_unescape(dest);
    output(o_start, _("Moving `%s' to `%s': "), unescaped_src, unescaped_dest);
    ne_free(unescaped_dest);
    ne_free(unescaped_src);
    out_result(ne_move(session.sess, get_bool_option(opt_overwrite), src, dest));
}

static void simple_copy(const char *src, const char *dest) 
{
    char *unescaped_src, *unescaped_dest;
    unescaped_src = ne_path_unescape(src);
    unescaped_dest = ne_path_unescape(dest);
    output(o_start, _("Copying `%s' to `%s': "), unescaped_src, unescaped_dest);
    ne_free(unescaped_dest);
    ne_free(unescaped_src);
    out_result(ne_copy(session.sess, get_bool_option(opt_overwrite), 
		       NE_DEPTH_INFINITE, src, dest));
}

/* Return full path of 'filename', relative to 'p'.  'p' must be
 * already URI-escaped; 'filename' must not be.  If 'iscoll' is
 * non-zero, the returned path will have a trailing slash. */
char *resolve_path(const char *p, const char *filename, int iscoll)
{
    char *ret, *pnt;
    if (*filename == '/') {
	/* It's absolute */
	ret = escape_path(filename);
    } else if (strcmp(filename, ".") == 0) {
	ret = ne_strdup(p);
    } else {
	pnt = escape_path(filename);
	ret = ne_concat(p, pnt, NULL);
	free(pnt);
    }
    if (iscoll && ret[strlen(ret)-1] != '/') {
	char *newret = ne_concat(ret, "/", NULL);
	free(ret);
	ret = newret;
    }
    /* Sort out '..', etc... */
    do {
	pnt = strstr(ret, "/../");
	if (pnt != NULL) {
	    char *last;
	    /* Find the *previous* path segment, to overwrite...
	     *    /foo/../
	     *    ^- last points here */
	    if (pnt > ret) {
		for(last = pnt-1; (last > ret) && (*last != '/'); last--);
	    } else {
		last = ret;
	    }
	    memmove(last, pnt + 3, strlen(pnt+2));
	} else {
	    pnt = strstr(ret, "/./");
	    if (pnt != NULL) {
		memmove(pnt, pnt+2, strlen(pnt+1));
	    }
	}
    } while (pnt != NULL);
    return ret;    
}

static void execute_get(const char *native_remote, const char *native_local)
{
    char *filename, *uri_path;
    int fd;

    uri_path = uri_resolve_native(native_remote);

    if (native_local) {
        filename = ne_strdup(native_local);
    }
    else {
        struct stat st;

        filename = ne_strdup(base_name(native_remote));

        /* Choose an appropriate local filename */
        if (stat(filename, &st) == 0) {
            char buf[BUFSIZ];
            /* File already exists... don't overwrite */
            snprintf(buf, BUFSIZ, _("Enter local filename for `%s': "),
                     native_remote);
            ne_free(filename);
            filename = readline(buf);
            if (filename == NULL || strlen(filename) == 0) {
                ne_free(uri_path);
                if (filename) ne_free(filename);
                printf(_("cancelled.\n"));
                return;
            }
        }
    }

    fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC|OPEN_BINARY_FLAGS|O_LARGEFILE, 
                  0644);
    output(o_download, _("Downloading `%s' to `%s':"), native_remote, filename);
    if (fd < 0) {
        output(o_finish, _("failed:\n%s\n"), strerror(errno));
    }
    else {
        int ret = ne_get(session.sess, uri_path, fd);
        if (close(fd) && ret == NE_OK) {
            int errnum = errno;
            ret = NE_ERROR;
            ne_set_error(session.sess, _("Could not write to file: %s"),
                         strerror(errnum));
        }
        out_result(ret);
    }

    ne_free(uri_path);
    ne_free(filename);
}

static void simple_put(const char *local, const char *remote)
{
    int fd = open(local, O_RDONLY | OPEN_BINARY_FLAGS | O_LARGEFILE);
    output(o_upload, _("Uploading %s to `%s':"), local, remote);
    if (fd < 0) {
	output(o_finish, _("Could not open file: %s\n"), strerror(errno));
    } else {
	out_result(ne_put(session.sess, remote, fd));
	(void) close(fd);
    }
}

static void execute_put(const char *local, const char *remote)
{
    char *uri_path = uri_resolve_native(remote ? remote : base_name(local));
    simple_put(local, uri_path);
    free(uri_path);
}

/* A bit like Haskell's map function... applies func to each
 * of the first argc items in argv */
static void 
map_multi(void (*func)(const char *), int argc, const char *argv[])
{
    int n;
    for(n = 0; n < argc; n++) {
	(*func)(argv[n]);
    }
}

static void multi_copy(int argc, const char *argv[])
{
    do_copymove(argc, argv, _("copying"), _("copy"), simple_copy);
}

static void multi_move(int argc, const char *argv[])
{
    do_copymove(argc, argv, _("moving"), _("move"), simple_move);
}

static void multi_mkcol(int argc, const char *argv[])
{
    map_multi(execute_mkcol, argc, argv);
}

static void multi_delete(int argc, const char *argv[])
{
    map_multi(execute_delete, argc, argv);
}

static void multi_rmcol(int argc, const char *argv[])
{
    map_multi(execute_rmcol, argc, argv);
}

static void multi_less(int argc, const char *argv[])
{
    map_multi(execute_less, argc, argv);
}

static void multi_cat(int argc, const char *argv[])
{
    map_multi(execute_cat, argc, argv);
}

/* this is getting too easy */
static void multi_mput(int argc, const char *argv[])
{
    for(; argv[0] != NULL; argv++) {
	char *uri_path = uri_resolve_native(argv[0]);
	simple_put(argv[0], uri_path);
	ne_free(uri_path);
    }    
}

static void multi_mget(int argc, const char *argv[])
{
    for(; argv[0] != NULL; argv++) {
	execute_get(argv[0], NULL);
    }
}

static void execute_chexec(const char *val, const char *native_path)
{
    static const ne_propname execprop = 
    { "http://apache.org/dav/props/", "executable" };
    /* Use a single operation; set the executable property to... */
    ne_proppatch_operation ops[] = { { &execprop, ne_propset, NULL }, { NULL } };
    char *uri_path = uri_resolve_native(native_path);
    ne_server_capabilities caps = {0};
    int ret;

    /* True or false, depending... */
    if (strcmp(val, "+") == 0) {
        ops[0].value = "T";
    }
    else if (strcmp(val, "-") == 0) {
        ops[0].value = "F";
    }
    else {
        printf(_("Use:\n"
                 "   chexec + %s   to make the resource executable\n"
                 "or chexec - %s   to make the resource unexecutable\n"),
                 native_path, native_path);
        return;
    }
    
    out_start(_("Setting isexecutable"), native_path);
    ret = ne_options(session.sess, uri_path, &caps);
    if (ret != NE_OK) {
        out_result(ret);
    }
    else if (!caps.dav_executable) {
        ne_set_error(session.sess,
                     _("The server does not support the 'isexecutable' property."));
        out_result(NE_ERROR);
    }
    else {
        out_result(ne_proppatch(session.sess, uri_path, ops));
    }
    ne_free(uri_path);
}

static void execute_lpwd(void)
{
    char pwd[BUFSIZ];
    if (getcwd(pwd, BUFSIZ) == NULL) {
	perror("pwd");
    } else {
	printf(_("Local directory: %s\n"), pwd);
    }
}


/* Using a hack, we get zero-effort multiple-argument 'lls' */
static void execute_lls(int argc, const char **argv)
{
    /* nasty cast; but these describe the same array of pointers, just
     * with different constness, so it should be okay. */
    char *const *vpargs = (char *const *)argv;
    int pid;
    pid = fork();
    switch (pid) {
    case 0: /* child...  */
	execvp("ls", &vpargs[-1]);
	printf(_("Error executing ls: %s\n"), strerror(errno));
	exit(-1);
	break;
    case -1: /* Error */
	printf(_("Error forking ls: %s\n"), strerror(errno));
	break;
    default: /* Parent */
	wait(NULL);
	break;
    }
    return;
}

static void execute_lcd(const char *p)
{
    const char *real_path;
    if (p) {
	real_path = p;
    } else {
	real_path = getenv("HOME");
	if (!real_path) {
	    printf(_("Could not determine home directory from environment.\n"));
	    return;
	}
    }
    if (chdir(real_path)) {
	printf(_("Could not change local directory:\nchdir: %s\n"),
	       strerror(errno));
    }
}

static void execute_pwd(void)
{
    char *uri = ne_uri_unparse(&session.uri);
    printf(_("Current collection is `%s'.\n"), uri);
    ne_free(uri);
}

static void execute_cd(const char *native_path)
{
    char *dest_uri_path, *uri_path = NULL;

    if (strcmp(native_path, "-") == 0) {
        if (!session.lastwp) {
            printf(_("No previous collection.\n"));
            return;
        }
        dest_uri_path = session.lastwp;
    }
    else {
        dest_uri_path = uri_path = uri_resolve_native_coll(native_path);
    }
    if (set_path(dest_uri_path) == 0) {
        /* Success */
        if (dest_uri_path == uri_path) {
            ne_free(session.lastwp);
        }
        session.lastwp = session.uri.path;
        session.uri.path = dest_uri_path;
    }
    else {
        /* Failure case, nothing else to do. */
        if (uri_path) ne_free(uri_path);
    }
}

static void display_help_message(void)
{
    unsigned int n, pos = 0;

    printf("Available commands: \n ");
    
    for (n = 0; commands[n].id != cmd_unknown; n++, pos=(pos+1)%7) {
        printf("%-11s", commands[n].name);
        if (pos == 6) printf("\n ");
#if 0        
	if (commands[n].call && commands[n].short_help) {
	    printf(" %-26s %s\n", commands[n].call, _(commands[n].short_help));
	}
#endif
    }

    if (pos == 6)
        putchar('\r');
    else
        putchar('\n');

    printf(_("Aliases: rm=delete, mkdir=mkcol, mv=move, cp=copy, "
	     "more=less, quit=exit=bye\n"));
}

static void execute_help(const char *arg)
{
    if (!arg) {
	display_help_message();
    } else {
	const struct command *cmd = get_command(arg);

	if (cmd) {
	    printf(_(" `%s'   %s\n"), cmd->call, _(cmd->short_help));
	    if (cmd->needs_connection) {
		printf(_("This command can only be used when connected to a server.\n"));
	    }
	} else {
	    printf(_("Command name not known: %s\n"), arg);
	}
    }
}

static void execute_echo(int count, const char **args)
{
    const char **pnt;
    for(pnt = args; *pnt != NULL; pnt++) {
	printf("%s ", *pnt);
    }
    putchar('\n');
}

void execute_about(void)
{
    printf("cadaver " PACKAGE_VERSION "\n%s\n", ne_version_string());
#ifdef HAVE_LIBREADLINE
    printf("readline %s\n", rl_library_version);
#endif
}

/* The T? macros are used to cast the function pointer into the
 * command structure.  Using GCC extensions this is done in a
 * type-safe way; for non-GCC< it's type-unsafe. */
#if defined(__GNUC__)
#define T0(x) {take0: x}
#define T1(x) {take1: x}
#define T2(x) {take2: x}
#define T3(x) {take3: x}
#define TV(x) {takeV: x}
#else
/* cast to be like first member of union. */
#define Tn(x) { ((void (*)(void)) x) }
#define T0(x) Tn(x)
#define T1(x) Tn(x)
#define T2(x) Tn(x)
#define T3(x) Tn(x)
#define TV(x) Tn(x)
#endif

/* C1: connected, 1-arg function C2: connected, 2-arg function
 * U0: disconnected, 0-arg function. */
#define C1(x,c,h) { cmd_##x, #x, true, 1, 1, parmscope_remote, T1(execute_##x),c,h }
#define U0(x,h) { cmd_##x, #x, false, 0, 0, parmscope_none, T0(execute_##x),#x,h }
#define UO1(x,c,h) { cmd_##x, #x, false, 0, 1, parmscope_none, T1(execute_##x),c,h }
#define C2M(x,c,h) { cmd_##x, #x, true, 2, CMD_VARY, parmscope_remote, TV(multi_##x),c,h }
#define C1M(x,c,h) { cmd_##x, #x, true, 1, CMD_VARY, parmscope_remote, TV(multi_##x),c,h }

/* commands[] is not static because it would mean adding a bunch of
 * prototypes for execute_* etc, and declaring this at the top of the
 * file. */

/* Separate structures for commands and command names. */
/* DON'T FORGET TO ADD A NEW COMMAND ALIAS WHEN YOU ADD A NEW COMMAND */
const struct command commands[] = {
    { cmd_ls, "ls", true, 0, 1, parmscope_remote, T1(execute_ls), 
      N_("ls [path]"), N_("List contents of current [or other] collection") },
    C1(cd, N_("cd path"), N_("Change to specified collection")),
    { cmd_pwd, "pwd", true, 0, 0, parmscope_none, T0(execute_pwd),
      "pwd", N_("Display name of current collection") },
    { cmd_put, "put", true, 1, 2, parmscope_none, T2(execute_put),
      N_("put local [remote]"), N_("Upload local file") },
    { cmd_get, "get", true, 1, 2, parmscope_none, T2(execute_get),
      N_("get remote [local]"), N_("Download remote resource") },
    C1M(mget, N_("mget remote..."), N_("Download many remote resources")),
    { cmd_mput, "mput", true, 1, CMD_VARY, parmscope_local, TV(multi_mput), 
      N_("mput local..."), N_("Upload many local files") },
    C1(edit, N_("edit resource"), N_("Edit given resource")),
    C1M(less, N_("less remote..."), N_("Display remote resource through pager")), 
    C1M(mkcol, N_("mkcol remote..."), N_("Create remote collection(s)")), 
    C1M(cat, N_("cat remote..."), N_("Display remote resource(s)")), 
    C1M(delete, N_("delete remote..."), N_("Delete non-collection resource(s)")),
    C1M(rmcol, N_("rmcol remote..."), N_("Delete remote collections and ALL contents")),
    C2M(copy, N_("copy source... dest"), N_("Copy resource(s) from source to dest")), 
    C2M(move, N_("move source... dest"), N_("Move resource(s) from source to dest")),
    
/* DON'T FORGET TO ADD A NEW COMMAND ALIAS WHEN YOU ADD A NEW COMMAND */

    /*** Locking commands ***/

    C1(lock, N_("lock resource"), N_("Lock given resource")),
    C1(unlock, N_("unlock resource"), N_("Unlock given resource")),
    C1(discover, N_("discover resource"), N_("Display lock information for resource")),
    C1(steal, N_("steal resource"), N_("Steal lock token for resource")),
    { cmd_showlocks, "showlocks", true, 0, 0, parmscope_none, T0(execute_showlocks),
      "showlocks", N_("Display list of owned locks") },

    /*** DeltaV commands ***/
    C1(version, N_("version resource"), N_("Place given resource under version control")),
    C1(checkin, N_("checkin resource"), N_("Checkin given resource")),
    C1(checkout, N_("checkout resource"), N_("Checkout given resource")),
    C1(uncheckout, N_("uncheckin resource"), N_("Uncheckout given resource")),
    C1(history, N_("history resource"), N_("Show version history of resource")),

    { cmd_label, "label", true, 3, 3, parmscope_none, T3(execute_label),
      N_("label res [add|set|remove] labelname"),
      N_("Set/Del/Edit label on resource") },


    /*** Property handling ***/
    C1(propnames, "propnames res", N_("Names of properties defined on resource")) ,

    { cmd_chexec, "chexec", true, 2, 2, parmscope_none, T2(execute_chexec),
      N_("chexec [+|-] remote"), N_("Change isexecutable property of resource") },
    
    { cmd_propget, "propget", true, 1, 2, parmscope_none, T2(execute_propget),
      N_("propget res [propname]"), 
      N_("Retrieve properties of resource") },
    { cmd_propdel, "propdel", true, 2, 2, parmscope_none, T2(execute_propdel),
      N_("propdel res propname"), 
      N_("Delete property from resource") },
    { cmd_propset, "propset", true, 3, 3, parmscope_none, T3(execute_propset),
      N_("propset res propname value"),
      N_("Set property on resource") },

    { cmd_search, "search", true, 1, CMD_VARY, parmscope_remote, TV(execute_search),
      N_("search query"), 
      N_("DASL Search resource in current collection\n\n"
	 " Examples:\n"
	 "   - search where content length is smaller than 100:\n"
	 "      > search getcontentlength < 100\n" 
         "   - search where author is Smith or Jones\n"
         "      > search author = Smith or author = Jones\n"
	 " Available operators and keywords:\n"
	 "     - and, or , (, ), =, <, >, <=, >=, like\n"
         " (See also variables searchdepth, searchorder, searchdorder)\n") },
    
    { cmd_set, "set", false, 0, 2, parmscope_none, T2(execute_set), 
      N_("set [option] [value]"), N_("Set an option, or display options") },
    { cmd_open, "open", false, 1, 1, parmscope_none, T1(open_connection), 
      "open URL", N_("Open connection to given URL") },
    { cmd_close, "close", true, 0, 0, parmscope_none, T0(close_connection), 
      "close", N_("Close current connection") },
    { cmd_echo, "echo", false, 1, CMD_VARY, parmscope_remote, TV(execute_echo), 
      "echo", NULL },
    { cmd_quit, "quit", false, 0, 1, parmscope_none, T1(NULL), "quit",
      N_("Exit program") },
    /* Unconnected operation, 1 mandatory argument */
    { cmd_unset, "unset", false, 1, 2, parmscope_none, T2(execute_unset), 
      N_("unset [option] [value]"), N_("Unsets or clears value from option.") },
    /* Unconnected operation, 0 arguments */
    UO1(lcd, N_("lcd [directory]"), N_("Change local working directory")), 
    { cmd_lls, "lls", false, 0, CMD_VARY, parmscope_local, TV(execute_lls), 
      N_("lls [options]"), N_("Display local directory listing") },
    U0(lpwd, N_("Print local working directory")),
    { cmd_logout, "logout", true, 0, 0, parmscope_none, T0(execute_logout), "logout",
      N_("Logout of authentication session") },
    UO1(help, N_("help [command]"), N_("Display help message")), 
    { cmd_describe, "describe", false, 1, 1, parmscope_none, T1(execute_describe),
      "describe option", N_("Describe an option variable") },
    U0(about, N_("Information about this version of cadaver") ),

/* DON'T FORGET TO ADD A NEW COMMAND ALIAS WHEN YOU ADD A NEW COMMAND. */

    { cmd_unknown, 0 } /* end-of-list marker, DO NOT move */
};    

