/* 
   'ls' for cadaver
   Copyright (C) 2000-2004, 2006, Joe Orton <joe@manyfish.co.uk>, 
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <time.h>

#include <ne_request.h>
#include <ne_props.h>
#include <ne_uri.h>
#include <ne_alloc.h>
#include <ne_dates.h>

#include "i18n.h"
#include "commands.h"
#include "cadaver.h"
#include "basename.h"
#include "utils.h"

struct fetch_context {
    struct resource **list;
    const char *target; /* Request-URI of the PROPFIND */
    unsigned int include_target; /* Include resource at href */
};    

static const ne_propname ls_props[] = {
    { "DAV:", "getcontentlength" },
    { "DAV:", "getlastmodified" },
    { "http://apache.org/dav/props/", "executable" },
    { "DAV:", "resourcetype" },
    { "DAV:", "checked-in" },
    { "DAV:", "checked-out" },
    { NULL }
};

#define ELM_resourcetype (NE_PROPS_STATE_TOP + 1)
#define ELM_collection (NE_PROPS_STATE_TOP + 2)

static const struct ne_xml_idmap ls_idmap[] = {
    { "DAV:", "resourcetype", ELM_resourcetype },
    { "DAV:", "collection", ELM_collection }
};

static int compare_resource(const struct resource *r1, 
			    const struct resource *r2)
{
    /* Sort errors first, then collections, then alphabetically */
    if (r1->type == resr_error) {
	return -1;
    } else if (r2->type == resr_error) {
	return 1;
    } else if (r1->type == resr_collection) {
	if (r2->type != resr_collection) {
	    return -1;
	} else {
	    return strcmp(r1->uri, r2->uri);
	}
    } else {
	if (r2->type != resr_collection) {
	    return strcmp(r1->uri, r2->uri);
	} else {
	    return 1;
	}
    }
}

static void display_ls_line(struct resource *res)
{
    const char *restype;
    char exec_char, vcr_char, *name;

    switch (res->type) {
    case resr_normal: restype = ""; break;
    case resr_reference: restype = _("Ref:"); break;
    case resr_collection: restype = _("Coll:"); break;
    default:
	restype = "???"; break;
    }
    
    if (ne_path_has_trailing_slash(res->uri)) {
	res->uri[strlen(res->uri)-1] = '\0';
    }
    name = strrchr(res->uri, '/');
    if (name != NULL && strlen(name+1) > 0) {
	name++;
    } else {
	name = res->uri;
    }

    name = ne_path_unescape(name);

    if (res->type == resr_error) {
	printf(_("Error: %-30s %d %s\n"), name, res->error_status,
	       res->error_reason?res->error_reason:_("unknown"));
    } else {
	exec_char = res->is_executable ? '*' : ' ';
	/* 0: no vcr, 1: checkin, 2: checkout */
	vcr_char = res->is_vcr==0 ? ' ' : (res->is_vcr==1? '>' : '<');
	printf("%5s %c%c%-29s %10" NE_FMT_TIME_T "  %s\n", 
	       restype, vcr_char, exec_char, name,
	       res->size, format_time(res->modtime));
    }

    free(name);
}

void execute_ls(const char *remote)
{
    int ret;
    char *real_remote;
    struct resource *reslist = NULL, *current, *next;

    if (remote != NULL) {
	real_remote = resolve_path(session.uri.path, remote, true);
    } else {
	real_remote = ne_strdup(session.uri.path);
    }
    out_start(_("Listing collection"), real_remote);
    ret = fetch_resource_list(session.sess, real_remote, 1, 0, &reslist);
    if (ret == NE_OK) {
	/* Easy this, eh? */
	if (reslist == NULL) {
	    output(o_finish, _("collection is empty.\n"));
	} else {
	    out_success();
	    for (current = reslist; current!=NULL; current = next) {
		next = current->next;
		if (strlen(current->uri) > strlen(real_remote)) {
		    display_ls_line(current);
		}
		free_resource(current);
	    }
	}
    } else {
	out_result(ret);
    }
    free(real_remote);
}

static void results(void *userdata, const char *uri,
		    const ne_prop_result_set *set)
{
    struct fetch_context *ctx = userdata;
    struct resource *current, *previous, *newres;
    const char *clength, *modtime, *isexec;
    const char *checkin, *checkout;
    const ne_status *status = NULL;
    ne_uri u;

    NE_DEBUG(NE_DBG_HTTP, "Uri: %s\n", uri);

    newres = ne_propset_private(set);
    
    if (ne_uri_parse(uri, &u))
	return;
    
    if (u.path == NULL) {
	ne_uri_free(&u);
	return;
    }

    NE_DEBUG(NE_DBG_HTTP, "URI path %s in %s\n", u.path, ctx->target);
    
    if (ne_path_compare(ctx->target, u.path) == 0 && !ctx->include_target) {
	/* This is the target URI */
	NE_DEBUG(NE_DBG_HTTP, "Skipping target resource.\n");
	/* Free the private structure. */
	free(newres);
	ne_uri_free(&u);
	return;
    }

    newres->uri = ne_strdup(u.path);

    clength = ne_propset_value(set, &ls_props[0]);    
    modtime = ne_propset_value(set, &ls_props[1]);
    isexec = ne_propset_value(set, &ls_props[2]);
    checkin = ne_propset_value(set, &ls_props[4]);
    checkout = ne_propset_value(set, &ls_props[5]);

    
    if (clength == NULL)
	status = ne_propset_status(set, &ls_props[0]);
    if (modtime == NULL)
	status = ne_propset_status(set, &ls_props[1]);

    if (newres->type == resr_normal && status) {
	/* It's an error! */
	newres->error_status = status->code;

	/* Special hack for Apache 1.3/mod_dav */
	if (strcmp(status->reason_phrase, "status text goes here") == 0) {
	    const char *desc;
	    if (status->code == 401) {
		desc = _("Authorization Required");
	    } else if (status->klass == 3) {
		desc = _("Redirect");
	    } else if (status->klass == 5) {
		desc = _("Server Error");
	    } else {
		desc = _("Unknown Error");
	    }
	    newres->error_reason = ne_strdup(desc);
	} else {
	    newres->error_reason = ne_strdup(status->reason_phrase);
	}
	newres->type = resr_error;
    }

    if (isexec && strcasecmp(isexec, "T") == 0) {
	newres->is_executable = 1;
    } else {
	newres->is_executable = 0;
    }

    if (modtime)
	newres->modtime = ne_httpdate_parse(modtime);

    if (clength)
	newres->size = atoi(clength);

    /* is vcr */
    if (checkin) {
	newres->is_vcr = 1;
    } else if (checkout) {
	newres->is_vcr = 2;
    } else {
	newres->is_vcr = 0;
    }

    NE_DEBUG(NE_DBG_HTTP, "End resource %s\n", newres->uri);

    for (current = *ctx->list, previous = NULL; current != NULL; 
	 previous = current, current=current->next) {
	if (compare_resource(current, newres) >= 0) {
	    break;
	}
    }
    if (previous) {
	previous->next = newres;
    } else {
	*ctx->list = newres;
    }
    newres->next = current;

    ne_uri_free(&u);
}

static int ls_startelm(void *userdata, int parent, 
                       const char *nspace, const char *name, const char **atts)
{
    ne_propfind_handler *pfh = userdata;
    struct resource *r = ne_propfind_current_private(pfh);
    int state = ne_xml_mapid(ls_idmap, NE_XML_MAPLEN(ls_idmap),
                             nspace, name);

    if (r == NULL || 
        !((parent == NE_207_STATE_PROP && state == ELM_resourcetype) ||
          (parent == ELM_resourcetype && state == ELM_collection)))
        return NE_XML_DECLINE;

    if (state == ELM_collection) {
	NE_DEBUG(NE_DBG_HTTP, "This is a collection.\n");
	r->type = resr_collection;
    }

    return state;
}

void free_resource(struct resource *res)
{
    NE_FREE(res->uri);
    NE_FREE(res->error_reason);
    free(res);
}

void free_resource_list(struct resource *res)
{
    struct resource *next;
    for (; res != NULL; res = next) {
	next = res->next;
	free_resource(res);
    }
}

static void *create_private(void *userdata, const char *uri)
{
    return ne_calloc(sizeof(struct resource));
}

int fetch_resource_list(ne_session *sess, const char *uri,
			 int depth, int include_target,
			 struct resource **reslist)
{
    ne_propfind_handler *pfh = ne_propfind_create(sess, uri, depth);
    int ret;
    struct fetch_context ctx = {0};
    
    *reslist = NULL;
    ctx.list = reslist;
    ctx.target = uri;
    ctx.include_target = include_target;

    ne_xml_push_handler(ne_propfind_get_parser(pfh), 
                        ls_startelm, NULL, NULL, pfh);

    ne_propfind_set_private(pfh, create_private, NULL);

    ret = ne_propfind_named(pfh, ls_props, results, &ctx);

    ne_propfind_destroy(pfh);

    return ret;
}
