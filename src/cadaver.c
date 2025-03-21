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

#include "config.h"

#include <sys/types.h>

#include <sys/time.h>
#include <sys/stat.h>

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif 
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#include <errno.h>

#ifdef NEED_SNPRINTF_H
#include "snprintf.h"
#endif

#include "i18n.h"

#include "getopt.h"
#include "getpass.h"

#ifdef ENABLE_NETRC
#include "netrc.h"
#endif

#include <ne_request.h>
#include <ne_auth.h>
#include <ne_basic.h>
#include <ne_string.h>
#include <ne_uri.h>
#include <ne_socket.h>
#include <ne_locks.h>
#include <ne_alloc.h>
#include <ne_redirect.h>

#include "common.h"
#include "cadaver.h"
#include "cmdline.h"
#include "commands.h"
#include "options.h"
#include "utils.h"

#define DEFAULT_NAMESPACE "http://webdav.org/cadaver/custom-properties/"

#ifdef ENABLE_NETRC
static netrc_entry *netrc_list; /* list of netrc entries */
#endif

/* Global state: */
const char *lock_store_fn = NULL;
static char *progname; /* argv[0] */
static char *rcfile;
static char *proxy_hostname;
static char *server_username, *server_password;

/* Current session state. */
struct session session;
static ne_ssl_client_cert *client_cert;

int tolerant; /* tolerate DAV-enabledness failure */
int in_completion; /* non-zero if in completion. */

/* Current output state */
static enum out_state {
    out_none, /* not doing anything */
    out_incommand, /* doing a simple command */
    out_transfer_upload, /* uploading a file, not yet started */
    out_transfer_download, /* downloading a file, not yet started */
    out_transfer_plain, /* doing a plain ... transfer */
    out_transfer_pretty, /* doing a pretty progress bar transfer */
    out_transfer_done /* a complete transfer */
} out_state;   

/* Prototypes */

static void quit_handler(int signo);

static void notifier(void *ud, ne_session_status status, 
                     const ne_session_status_info *info);
static void pretty_progress_bar(ne_off_t progress, ne_off_t total);
static int supply_creds_server(void *userdata, const char *realm, int attempt,
			       char *username, char *password);
static int supply_creds_proxy(void *userdata, const char *realm, int attempt,
			      char *username, char *password);

static void usage(void)
{
    printf(_(
"Usage: %s [OPTIONS] URL\n"
"  URL must be an absolute URI using the http: or https: scheme.\n"
"Options:\n"
"  -t, --tolerant            Allow cd/open into non-WebDAV enabled collection.\n"
"  -r, --rcfile=FILE         Read script from FILE instead of ~/.cadaverrc.\n"
"  -p, --proxy=PROXY[:PORT]  Use proxy host PROXY and optional proxy port PORT.\n"
"  -V, --version             Display version information.\n"
"  -h, --help                Display this help message.\n"
"Please send bug reports and feature requests via <https://github.com/notroj/cadaver>\n"), progname);
}

static void init_locking(void)
{
    lock_store_fn = get_option(opt_lockstore);
    session.locks = ne_lockstore_create();
    /* TODO: read in lock list from ~/.davlocks */
}

static void finish_locking(void)
{
    /* TODO: write out lock list to ~/.davlocks */
}

void close_connection(void)
{
    if (session.sess) ne_session_destroy(session.sess);
    session.sess = NULL;
    if (session.connected && session.uri.host)
        printf(_("Connection to `%s' closed.\n"), session.uri.host);
    session.connected = false;
    ne_uri_free(&session.uri);
    if (session.lastwp)
        ne_free(session.lastwp);
}

/* Sets the current collection to the given URI path.  Returns zero on
 * success, non-zero if newpath is an untolerated non-WebDAV
 * collection. */
int set_path(const char *uri_path)
{
    int is_coll = (getrestype(uri_path) == resr_collection);
    if (is_coll || tolerant) {
	if (!is_coll) {
	    session.isdav = 0;
	    printf(_("Ignored error: %s not WebDAV-enabled:\n%s\n"), uri_path,
		   ne_get_error(session.sess));
	} else {
	    session.isdav = 1;
	}
	return 0;
    }
    else {
	printf(_("Could not access %s (not WebDAV-enabled?):\n%s\n"), uri_path,
	       ne_get_error(session.sess));
	return 1;
    }
}

static int cert_verify(void *ud, int failures, const ne_ssl_certificate *c)
{
    char *tmp, from[NE_SSL_VDATELEN], to[NE_SSL_VDATELEN];
    const char *ident;

    ident = ne_ssl_cert_identity(c);

    if (ident)
        printf(_("WARNING: Untrusted server certificate presented for `%s':\n"),
               ident);
    else
        puts(_("WARNING: Untrusted server certificate presented:\n"));

#if NE_VERSION_MAJOR > 0 || NE_VERSION_MINOR > 31
    tmp = ne_ssl_cert_hdigest(c, NE_HASH_SHA256|NE_HASH_COLON);
    if (tmp) {
        printf(_("Server certificate SHA-256 digest: %s\n"), tmp);
        ne_free(tmp);
    }
#endif

    if (failures & NE_SSL_IDMISMATCH) {
	printf(_("Certificate was issued to hostname `%s' rather than `%s'\n"),
	       ne_ssl_cert_identity(c), session.uri.host);
	printf(_("This connection could have been intercepted.\n"));
    }

#define PRINT_AND_FREE(str, dn) \
tmp = ne_ssl_readable_dname(dn); printf(str, tmp); free(tmp)

    PRINT_AND_FREE(_("Issued to: %s\n"), ne_ssl_cert_subject(c));
    PRINT_AND_FREE(_("Issued by: %s\n"), ne_ssl_cert_issuer(c));

    ne_ssl_cert_validity(c, from, to);
    printf(_("Certificate is valid from %s to %s\n"), from, to);

    if (isatty(STDIN_FILENO)) {
	printf(_("Do you wish to accept the certificate? (y/n) "));
	return !yesno();
    } else {
	printf(_("Certificate rejected.\n"));
	return -1;
    }
}

static void provide_clicert(void *userdata, ne_session *sess,
                            const ne_ssl_dname *const *dname, int dncount)
{
    const char *ccfn = userdata;
    int n;

    printf("The server has requested a client certificate.\n");

#if 0
    /* display CA names? */
    for (n = 0; n < dncount; n++) {
        char *dn = ne_ssl_readable_dname(dname[n]);
        printf("Name: %s\n", dn);
        free(dn);
    }
#endif
    
    if (ne_ssl_clicert_encrypted(client_cert)) {
        const char *name = ne_ssl_clicert_name(client_cert);
        char *pass;
        
        if (!name) name = ccfn;
        
        printf("Client certificate `%s' is encrypted.\n", name);
        
        for (n = 0; n < 3; n++) {
            pass = fm_getpassword(_("Decryption password: "));
            if (pass == NULL) break;
            if (ne_ssl_clicert_decrypt(client_cert, pass)) {
                printf("Password incorrect, try again.\n");
            } else {
                break;
            }
        }
    }
    
    if (!ne_ssl_clicert_encrypted(client_cert)) {
        printf("Using client certificate.\n");
        ne_ssl_set_clicert(session.sess, client_cert);
    }
    
}

static void setup_ssl(ne_session *sess)
{
    char *ccfn = get_option(opt_clicert);

    ne_ssl_trust_default_ca(sess);
	      
    ne_ssl_set_verify(sess, cert_verify, NULL);

    if (ccfn) {
        client_cert = ne_ssl_clicert_read(ccfn);
        if (client_cert) {
            ne_ssl_provide_clicert(sess, provide_clicert, ccfn);
        }
        else {
            printf(_("Could not load client certificate from `%s'.\n"), ccfn);
        }
    }
}

void open_connection(const char *url)
{
    const char *proxy_host = get_option(opt_proxy);
    int ret;
    ne_session *sess = NULL;
    unsigned int proxy_port = 8080;

    close_connection();

    /* Parse the URL */
    if (ne_uri_parse(url, &session.uri) || session.uri.path == NULL
        || session.uri.scheme == NULL || session.uri.host  == NULL) {
        printf(_("Could not parse URL `%s'\n"), url);
        goto fail;
    }

    if (!session.uri.port)
        session.uri.port = ne_uri_defaultport(session.uri.scheme);

    /* Collections must be slash-terminated to avoid a redirect
     * round-trip. */
    if (!ne_path_has_trailing_slash(session.uri.path)) {
        char *pnt = ne_concat(session.uri.path, "/", NULL);
        ne_free(session.uri.path);
        session.uri.path = pnt;
    }

    sess = ne_session_create(session.uri.scheme, session.uri.host, session.uri.port);
    session.sess = sess;

    if (ne_strcasecmp(session.uri.scheme, "https") == 0) {
        if (!ne_has_support(NE_FEATURE_SSL)) {
            printf(_("No SSL/TLS support, cannot use URL `%s'\n"), url);
            goto fail;
        }
        setup_ssl(sess);
    }
    else if (ne_strcasecmp(session.uri.scheme, "http")) {
        printf(_("URL scheme '%s' not supported.\n"), session.uri.scheme);
        goto fail;
    }

    ne_lockstore_register(session.locks, sess);
    ne_redirect_register(sess);
    ne_set_notifier(sess, notifier, NULL);
    ne_set_session_flag(sess, NE_SESSFLAG_PERSIST, get_bool_option(opt_keepalive));
    ne_set_session_flag(sess, NE_SESSFLAG_EXPECT100, get_bool_option(opt_expect100));

    /* Get the proxy details */
    if (proxy_host && get_option(opt_proxy_port) != NULL) {
        proxy_port = atoi(get_option(opt_proxy_port));
    }

#ifdef ENABLE_NETRC
    {
	netrc_entry *found;
	found = search_netrc(netrc_list, session.uri.host);
	if (found != NULL) {
	    if (found->account && found->password) {
		server_username = found->account;
		server_password = found->password;
	    }
	}
    }
#endif /* ENABLE_NETRC */
    session.connected = 0;

    ne_set_useragent(session.sess, "cadaver/" PACKAGE_VERSION);
    ne_set_server_auth(session.sess, supply_creds_server, NULL);
    ne_set_proxy_auth(session.sess, supply_creds_proxy, NULL);

    if (get_bool_option(opt_systemproxy)) {
        ne_session_system_proxy(session.sess, 0);
    }
    else if (proxy_host) {
        if (proxy_hostname) ne_free(proxy_hostname);
        proxy_hostname = ne_strdup(proxy_host);
        ne_session_proxy(session.sess, proxy_host, proxy_port);
    }

    session.caps = 0;
    ret = ne_options2(session.sess, session.uri.path, &session.caps);

    switch (ret) {
    case NE_OK:
        if ((session.caps & NE_CAP_DAV_CLASS1) == 0) {
            printf(_("%s: Location does not advertise WebDAV class 1 support.\n"),
                   tolerant ? _("Warning") : _("Error"));
            if (!tolerant) break;
        }
	if (set_path(session.uri.path) == 0) {
            session.connected = true;
            return;
        }
        break;
    case NE_CONNECT:
	if (proxy_host) {
	    printf(_("Could not connect to `%s' on port %d:\n%s\n"),
		   proxy_hostname, proxy_port, ne_get_error(session.sess));
	} else {
	    printf(_("Could not connect to `%s' on port %d:\n%s\n"),
		   session.uri.host, session.uri.port, ne_get_error(session.sess));
	}
	break;
    case NE_LOOKUP:
	puts(ne_get_error(session.sess));
	break;
    default:
	printf(_("Could not open collection:\n%s\n"),
	       ne_get_error(session.sess));
	break;
    }

fail:
    close_connection();
}
       
/* Sets proxy server from hostport argument */    
static void set_proxy(const char *str)
{
    char *hostname = ne_strdup(str), *pnt;

    pnt = strchr(hostname, ':');
    if (pnt != NULL) {
	*pnt++ = '\0';
        if (*pnt)
            set_option(opt_proxy_port, ne_strdup(pnt));
    }
    set_option(opt_proxy, (void *)hostname);
}

static void parse_args(int argc, char **argv)
{
    static const struct option opts[] = {
	{ "version", no_argument, NULL, 'V' },
	{ "help", no_argument, NULL, 'h' },
	{ "proxy", required_argument, NULL, 'p' },
	{ "tolerant", no_argument, NULL, 't' },
	{ "rcfile", required_argument, NULL, 'r' },
	{ 0, 0, 0, 0 }
    };
    int optc;
    while ((optc = getopt_long(argc, argv, "ehtp:r:V", opts, NULL)) != -1) {
	switch (optc) {
	case 'h': usage(); exit(-1);
	case 'V': execute_about(); exit(-1);
	case 'p': set_proxy(optarg); break;
	case 't': tolerant = 1; break;
	case 'r': rcfile = strdup(optarg); break;
	case '?': 
	default:
	    printf(_("Try `%s --help' for more information.\n"), progname);
	    exit(-1);
	}
    }
    if (optind == (argc-1)) {
	open_connection(argv[optind]);
#ifdef HAVE_ADD_HISTORY
	{ 
	    char *run_cmd;
	    run_cmd = ne_concat("open ", argv[optind], NULL);
	    add_history(run_cmd);
	    free(run_cmd);
	}
#endif
    } else if (optind < argc) {
	usage();
	exit(-1);
    }
}

static char *read_command(void)
{
    char prompt[BUFSIZ];

    if (session.uri.path) {
        char *p = native_path_from_uri(session.uri.path);
        ne_snprintf(prompt, BUFSIZ, "dav:%s%c ", p,
                    session.isdav ? '>' : '?');
        ne_free(p);
    }
    else {
        ne_strnzcpy(prompt, "dav:!> ", sizeof prompt);
    }

    return readline(prompt); 
}

static int execute_command(const char *line)
{
    const struct command *cmd;
    char **tokens;
    int n, argcount, ret = 0;
    tokens = parse_command(line, &argcount);
    if (argcount == 0) {
	free(tokens);
	return 0;
    }
    argcount--;
    cmd = get_command(tokens[0]);
    if (cmd == NULL) {
	printf(_("Unrecognised command. Type 'help' for a list of commands.\n"));
    } else if (argcount < cmd->min_args) {
	printf(_("The `%s' command requires %d argument%s"),
		tokens[0], cmd->min_args, cmd->min_args==1?"":"s");
	if (cmd->short_help) {
	    printf(_(":\n  %s : %s\n"), cmd->call, cmd->short_help);
	} else {
	    printf(".\n");
	}
    } else if (argcount > cmd->max_args) {
	if (cmd->max_args) {
	    printf(_("The `%s' command takes at most %d argument%s"), tokens[0],
		    cmd->max_args, cmd->max_args==1?"":"s");
	} else {
	    printf(_("The `%s' command takes no arguments"), tokens[0]);
	}	    
	if (cmd->short_help) {
	    printf(_(":\n" "  %s : %s\n"), cmd->call, cmd->short_help);
	} else {
	    printf(".\n");
	}
    } else if (!session.connected && cmd->needs_connection) {
	printf(_("The `%s' command can only be used when connected to the server.\n"
		  "Try running `open' first (see `help open' for more details).\n"), 
		  tokens[0]);
    } else if (cmd->id == cmd_quit) {
	ret = -1;
    } else {
	/* Cast away */
	/* with a nod in the general direction of apache */
	switch (cmd->max_args) {
	case 0: cmd->handler.take0(); break;
	case 1: /* tokens[1]==NULL if argcount==0 */
	    cmd->handler.take1(tokens[1]); break; 
	case 2: 
	    if (argcount <=1) {
		cmd->handler.take2(tokens[1], NULL);
	    } else {
		cmd->handler.take2(tokens[1], tokens[2]);
	    }
	    break;
	case 3:
	    cmd->handler.take3(tokens[1], tokens[2], tokens[3]);
	    break;
	case CMD_VARY:
	    cmd->handler.takeV(argcount, (const char **) &tokens[1]);
	default:
	    break;
	}
    }
    for (n = 0; n < argcount; n++) {
        ne_free(tokens[n]);
    }
    ne_free(tokens);
    return ret;
}

static void quit_handler(int sig)
{
    /* Reinstall handler */
    if (child_running) {
	/* The child gets the signal anyway... it can deal with it.
	 * Proper way is probably to ignore signals while child is
	 * running? */
	signal(sig, quit_handler);
	return;
    } else {
	printf(_("Terminated by signal %d.\n"), sig);
	if (session.connected) {
	    close_connection();
	}
	exit(-1);
    }
}

static void init_signals(void)
{
    signal(SIGTERM, quit_handler);
    signal(SIGABRT, quit_handler);
    signal(SIGQUIT, quit_handler);
    signal(SIGINT, quit_handler);
}

static void init_netrc(void)
{
#ifdef ENABLE_NETRC
    char *netrc = ne_concat(getenv("HOME"), "/.netrc", NULL);
    netrc_list = parse_netrc(netrc);
#endif
}

static int init_rcfile(void)
{
    int ret = 0;
    char buf[BUFSIZ];
    struct stat st;
    FILE *f;

    if (rcfile == NULL) {
	rcfile = ne_concat(getenv("HOME"), "/.cadaverrc", NULL);

	if (stat(rcfile, &st) != 0) {
	    NE_DEBUG(DEBUG_FILES, "No rcfile\n");
	    free(rcfile);
	    return 0;
	}
    }

    f = fopen(rcfile, "r");
    if (f == NULL) {
        printf(_("Could not read rcfile %s: %s\n"), rcfile, 
	   strerror(errno));
    } else {
	for (;;) {
	    if (fgets(buf, BUFSIZ, f) != NULL) {
		ret = execute_command(ne_shave(buf, "\r\n"));
		if (ret != 0)
		    break;
	    } else {
		break;
	    }
	}
	fclose(f);
    }
    free(rcfile);
    return ret;
}


#ifdef HAVE_LIBREADLINE

#define COMPLETION_CACHE_EXPIRE 10 /* seconds */

#ifndef HAVE_RL_COMPLETION_MATCHES
/* readline <4.2 compatibility. */
#define rl_completion_matches completion_matches
#define rl_filename_completion_function filename_completion_function
#endif

/* The remote completion generator is invoked multiple times to return
 * all possible matches; the remote collection listing is cached to
 * allow this, and for any future attempts at tab-completion within
 * the same collection. */
struct completion_cache {
    struct resource *list;
    time_t expiry;
    char *path; /* URI path. */
};

static void refresh_completion_cache(const char *uri_path,
                                     struct completion_cache *cache)
{
    if (cache->expiry < time(NULL) || strcmp(uri_path, cache->path)) {
        if (cache->expiry) {
            ne_free(cache->path);
            free_resource_list(cache->list);
        }

        if (fetch_resource_list(session.sess, uri_path, 1, 0,
                                &cache->list) == NE_OK) {
            cache->expiry = time(NULL) + COMPLETION_CACHE_EXPIRE;
            cache->path = ne_strdup(uri_path);
        }
        else {
            memset(cache, 0, sizeof *cache);
        }
    }
}

/* Remote filename completion generator. */
static char *remote_completion(const char *text, int state)
{
    static struct completion_cache cache;
    static struct resource *current;
    static char *text_uri_path;
    static size_t text_uri_len;
    size_t sup_len = strlen(session.uri.path);
    char *ret;

    if (state == 0) {
        /* For the initial state, refresh the completion cache after
         * determining the root path. */
        const char *sep;
        char *uri_root_path;

        /* Convert the input text to URI form for comparison against
         * the URI form of the paths within the collection. */
        if (text_uri_path) ne_free(text_uri_path);
        text_uri_path = uri_resolve_native(text);
        text_uri_len = strlen(text_uri_path);

        /* If there is a path segment in the input text, use that to
         * resolve a collection name relative to the session URI,
         * otherwise resolve against the current URI. */
        if ((sep = strrchr(text, '/')) != NULL) {
            char *native_root = ne_strndup(text, sep-text);
            uri_root_path = uri_resolve_native_coll(native_root);
            ne_free(native_root);
        }
        else {
            uri_root_path = ne_strdup(session.uri.path);
        }

        refresh_completion_cache(uri_root_path, &cache);
        current = cache.list;
        ne_free(uri_root_path);
    }

    /* For the first and subsequent invocation, iterate from the
     * current position in the resource list until a match for the
     * input text is found. */
    for (ret = NULL; current && !ret; current = current->next) {
        if (strncmp(text_uri_path, current->uri, text_uri_len) == 0) {
            /* If matching, return the path without the current path
             * prefix if the URI path is below the current path, else
             * return the absolute form. */
            if (strlen(current->uri) > sup_len
                && strncmp(current->uri, session.uri.path, sup_len) == 0)
                ret = native_path_from_uri(current->uri + sup_len);
            else
                ret = native_path_from_uri(current->uri);
        }
    }

    return ret;
}

static char **completion(const char *text, int start, int end)
{
    char **matches = NULL;
    char *sep = strchr(rl_line_buffer, ' ');

    in_completion = 1;

    if (start == 0) {
        matches = rl_completion_matches(text, command_generator);
    }
    else if (sep != NULL) {
        char *cname = ne_strndup(rl_line_buffer, sep - rl_line_buffer);
        const struct command *cmd = get_command(cname);
        enum command_scope scope = cmd ? cmd->scope : parmscope_none;

        ne_free(cname);

        switch (scope) {
        case parmscope_none:
            break;
        case parmscope_local:
            matches = rl_completion_matches(text,
                                            rl_filename_completion_function);
            break;
        case parmscope_option:
            /* TODO */
            break;
        case parmscope_remote:
            if (session.connected) {
                matches = rl_completion_matches(text, remote_completion);
            }
            break;
        }
    }

    in_completion = 0;

    return matches;
}

#endif /* HAVE_LIBREADLINE */

void output(enum output_type t, const char *fmt, ...)
{
    va_list params;
    if (t == o_finish) {
	switch (out_state) {
	case out_transfer_plain:
	    printf("] ");
	    break;
	default:
	    putchar(' ');
	    break;
	}
    }
    va_start(params, fmt);
    vfprintf(stdout, fmt, params);
    va_end(params);
    fflush(stdout);
    switch (t) { 
    case o_start:
	out_state = out_incommand;
	break;
    case o_upload:
	out_state = out_transfer_upload;
	break;
    case o_download:
        out_state = out_transfer_download;
        break;
    case o_finish:
	out_state = out_none;
	break;
    }
}

static void init_readline(void)
{
#ifdef HAVE_LIBREADLINE
    rl_readline_name = "cadaver";
    rl_attempted_completion_function = completion;
#endif /* HAVE_LIBREADLINE */
}

#ifndef HAVE_LIBREADLINE
char *readline(const char *prompt)
{
    static char buf[256];
    char *ret;
    if (prompt) {
	printf("%s", prompt);
    }
    ret = fgets(buf, 256, stdin);
    if (ret) {
	return ne_strdup(ne_shave(buf, "\r\n"));
    } else {
	return NULL;
    }
}
#endif

static void init_options(void)
{
    char *lockowner;
    char *user = getenv("USER"), *hostname = getenv("HOSTNAME");
    int utf8_default;
    
    if (user && hostname) {
	/* set this here so they can override it */
	lockowner = ne_concat("mailto:", user, "@", hostname, NULL);
	set_option(opt_lockowner, lockowner);
    } else {
	set_option(opt_lockowner, NULL);
    }

    set_option(opt_editor, NULL);
    set_option(opt_namespace, ne_strdup(DEFAULT_NAMESPACE));
    set_bool_option(opt_overwrite, 1);
    set_bool_option(opt_quiet, 1);
    set_bool_option(opt_searchall, 1);
    lockdepth = NE_DEPTH_INFINITE;
    lockscope = ne_lockscope_exclusive;
    searchdepth = NE_DEPTH_INFINITE;

    /* Detect whether it's possible to directly output UTF-8. */
#ifdef HAVE_LANGINFO_CODESET
    out_charset = nl_langinfo(CODESET);
    utf8_default = strcmp(out_charset, "UTF-8") == 0;
#else
    {
        char *tmp;

        utf8_default = ((tmp = getenv("LC_ALL")) ||
                        || (tmp = getenv("LC_CTYPE"))
                        || (tmp = getenv("LANG")))
            && strstr(tmp, "UTF-8");
    }
#endif

#ifndef HAVE_ICONV
    if (!utf8_default) {
        fprintf(stderr, _("cadaver: Error: cadaver can only run in a locale "
                          "using the UTF-8 character encoding since iconv support "
                          "was not detected at build time.\n"));
        exit(EXIT_FAILURE);
    }
#endif

    set_option(opt_utf8, &utf8_default);
}

int main(int argc, char *argv[])
{
    int ret = 0;
    char *home = getenv("HOME"), *tmp;

    progname = argv[0];

#ifdef HAVE_SETLOCALE
    setlocale(LC_ALL, "");
#endif

#ifdef ENABLE_NLS
    bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    textdomain(PACKAGE_NAME);
#endif /* ENABLE_NLS */

    ne_debug_init(stderr, 0);
    if (!home) {
	/* Show me the way to go home... */
	printf(_("Environment variable $HOME needs to be set!\n"));
	return -1;
    }

    ne_sock_init();

    memset(&session, 0, sizeof session);

    /* Options before rcfile, so rcfile settings can
     * override defaults */
    tmp = ne_concat(home, "/.cadaver-locks", NULL);
    set_option(opt_lockstore, tmp);
    init_options();
    init_netrc();

    init_signals();
    init_locking();
    
    parse_args(argc, argv);

    ret = init_rcfile();

    init_readline();

    while (ret == 0) {
	char *cmd;
	cmd = read_command();
	if (cmd == NULL) {
	    /* Is it safe to do this... they just closed stdin, so
	     * is it bad to write to stdout? */
	    putchar('\n');
	    ret = 1;
	} else {
#ifdef HAVE_ADD_HISTORY
	    if (strcmp(cmd, "") != 0) add_history(cmd);
#endif
	    ret = execute_command(cmd);
	    free(cmd);
	}
    }

    if (session.connected) {
	close_connection();
    }

    finish_locking();

    ne_sock_exit();

    return 0;
}

static void notifier(void *ud, ne_session_status status, const ne_session_status_info *info)
{
    int quiet = get_bool_option(opt_quiet);

    if (in_completion) return; /* do nothing during tab-completion */

    switch (out_state) {
    case out_none:
        if (quiet) break;

	switch (status) {
	case ne_status_lookup:
	    printf(_("Looking up hostname... "));
	    break;
	case ne_status_connecting:
	    printf(_("Connecting to server... "));
	    break;
	case ne_status_connected:
	    printf(_("connected.\n"));
	    break;
        default:
            break;
	}
	break;
    case out_incommand:
    case out_transfer_upload:
    case out_transfer_download:
    case out_transfer_done:
	switch (status) {
	case ne_status_connecting:
            if (!quiet) printf(_(" (reconnecting..."));
            /* FIXME: should reset out_state here if transfer_done */
	    break;
	case ne_status_connected:
	    if (!quiet) printf(_("done)"));
	    break;
        case ne_status_recving:
        case ne_status_sending:
            /* Start of transfer: */
            if ((out_state == out_transfer_download 
                 && status == ne_status_recving)
                || (out_state == out_transfer_upload 
                    && status == ne_status_sending)) {
                if (isatty(STDOUT_FILENO) && info->sr.total > 0) {
                    out_state = out_transfer_pretty;
                    putchar('\n');
                    pretty_progress_bar(info->sr.progress, info->sr.total);
                } else {
                    out_state = out_transfer_plain;
                    printf(" [.");
                }
            }
            break;                
        default:
            break;
	}
	break;
    case out_transfer_plain:
	switch (status) {
	case ne_status_connecting:
	    printf(_("] reconnecting: "));
	    break;
	case ne_status_connected:
	    printf(_("okay ["));
	    break;
        case ne_status_sending:
        case ne_status_recving:
            putchar('.');
            fflush(stdout);
            if (info->sr.progress == info->sr.total) {
                out_state = out_transfer_done;
            }
            break;
        default:
            break;
	}
	break;
    case out_transfer_pretty:
	switch (status) {
	case ne_status_connecting:
	    if (!quiet) {
                putchar('\r');
                printf(_("Transfer timed out, reconnecting... "));
            }
	    break;
	case ne_status_connected:
	    if (!quiet) printf(_("okay."));
	    break;
        case ne_status_recving:
        case ne_status_sending:
	    pretty_progress_bar(info->sr.progress, info->sr.total);
            if (info->sr.progress == info->sr.total) {
                out_state = out_transfer_done;
            }
        default:
            break;
	}
	break;	
    }
    fflush(stdout);
}

/* From ncftp.
   This function is (C) 1995 Mike Gleason, (mgleason@NcFTP.com)
 */
static void 
sub_timeval(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{
    tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
    tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
    if (tdiff->tv_usec < 0) {
	tdiff->tv_sec--;
	tdiff->tv_usec += 1000000;
    }
}

/* Smooth progress bar.
 * Doesn't update the bar more than once every 100ms, since this 
 * might give flicker, and would be bad if we are displaying on
 * a slow link anyway.
 */
static void pretty_progress_bar(ne_off_t progress, ne_off_t total)
{
    int len, n;
    double pc;
    static struct timeval last_call = {0};
    struct timeval this_call;
    
    if (total < 0)
	return;

    if (progress < total && gettimeofday(&this_call, NULL) == 0) {
	struct timeval diff;
	sub_timeval(&diff, &this_call, &last_call);
	if (diff.tv_sec == 0 && diff.tv_usec < 100000) {
	    return;
	}
	last_call = this_call;
    }
    if (progress == 0 || total == 0) {
	pc = 0;
    } else {
	pc = (double)progress / total;
    }
    len = pc * 30;
    putchar('\r');
    printf(_("Progress: ["));
    for (n = 0; n<30; n++) {
	putchar((n<len-1)?'=':
		 (n==(len-1)?'>':' '));
    }
    printf(_("] %5.1f%% of %" NE_FMT_NE_OFF_T " bytes"), pc*100, total);
    fflush(stdout);
}

static int supply_creds(const char *prompt, const char *realm, const char *hostname,
			char *username, char *password)
{
    char *tmp;

    switch (out_state) {
    case out_transfer_pretty:
    case out_transfer_done:
	putchar('\n');
        break;
    case out_none:
	break;
    case out_incommand:
    case out_transfer_upload:
    case out_transfer_download:
	putchar(' ');
	break;
    case out_transfer_plain:
	printf("] ");
	break;
    }
    printf(prompt, realm, hostname);
    
    tmp = readline(_("Username: "));
    if (tmp == NULL) {
	putchar('\r'); printf(_("Authentication aborted!\n"));
	return -1;
    } else if (strlen(tmp) >= NE_ABUFSIZ) {
	putchar('\r'); printf(_("Username too long (>%d)\n"), NE_ABUFSIZ);
	free(tmp);
	return -1;
    }

    strcpy(username, tmp);
    free(tmp);

    tmp = fm_getpassword(_("Password: "));
    if (tmp == NULL) {
	printf(_("Authentication aborted!\n"));
	return -1;
    } else if (strlen(tmp) >= NE_ABUFSIZ) {
	putchar('\r'); printf(_("Password too long (>%d)\n"), NE_ABUFSIZ);
	return -1;
    }
    
    strcpy(password, tmp);
	
    switch (out_state) {
    case out_transfer_download:
    case out_transfer_upload:
    case out_transfer_done:
    case out_incommand:
	printf(_("Retrying:"));
	fflush(stdout);
	break;
    case out_transfer_plain:
	printf(_("Retrying ["));
	fflush(stdout);
	break;
    default:
	break;
    }
    return 0;
}

static int supply_creds_server(void *userdata, const char *realm, int attempt,
			       char *username, char *password)
{
    /* Try netrc creds if we have them on first auth attempt. */
    if (server_username && server_password && attempt-- == 0) {
	ne_strnzcpy(username, server_username, NE_ABUFSIZ);
	ne_strnzcpy(password, server_password, NE_ABUFSIZ);
	return 0;
    }

    if (attempt > 1)
	return -1;

    return supply_creds(
	_("Authentication required for %s on server `%s':\n"), realm,
	session.uri.host, username, password);
}

static int supply_creds_proxy(void *userdata, const char *realm, int attempt,
			      char *username, char *password) 
{
    if (attempt > 1)
	return -1;

    return supply_creds(
	_("Authentication required for %s on proxy server `%s':\n"), realm,
	proxy_hostname, username, password);
}

