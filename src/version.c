/* 
   'version' for cadaver
   Copyright (C) 2003-2005, Joe Orton <joe@manyfish.co.uk>
   Copyright (C) 2002-2003, GRASE Lab, UCSC <grase@cse.ucsc.edu>, 
                                                                     
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
#include <ne_basic.h>
#include <ne_props.h>
#include <ne_uri.h>
#include <ne_alloc.h>
#include <ne_dates.h>

#include "i18n.h"
#include "commands.h"
#include "cadaver.h"
#include "basename.h"
#include "utils.h"

/* Message body for REPORT */
static const char *report_body = 
"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
"<D:version-tree xmlns:D=\"DAV:\">"
" <D:prop>"
"  <D:version-name/>"
"  <D:creator-displayname/>"
"  <D:getcontentlength/>"
"  <D:getlastmodified/>"
"  <D:successor-set/>"
" </D:prop>"
"</D:version-tree>";

typedef struct report_res
{
    char *href;
    
    /* live props */
    char *version_name;
    char *creator_displayname;
    char *getcontentlength;
    char *getlastmodified;
    char *successor_set;

    struct report_res *next;
}
report_res;


/* Search XML parser context */
typedef struct
{
    report_res *root;
    report_res *curr;
    int result_num;
    int start_prop;
    int err_code;
    
    ne_buffer *cdata;
}
report_ctx;
    
enum
{
    ELEM_multistatus = 1,
    ELEM_response,
    ELEM_href,
    ELEM_prop,
    ELEM_propstat,
    ELEM_status,

    /* props from RFC 2518 , 23 Appendices 23.1 */
    ELEM_version_name,
    ELEM_creator_displayname,
    ELEM_getcontentlength,
    ELEM_getlastmodified,

    ELEM_ignore
};


static const struct ne_xml_idmap report_elements[] = {
    {"DAV:", "multistatus", ELEM_multistatus},
    {"DAV:", "response", ELEM_response},
    {"DAV:", "href", ELEM_href},
    {"DAV:", "propstat", ELEM_propstat},
    {"DAV:", "prop", ELEM_prop},
    {"DAV:", "status", ELEM_status},

    /* Live props */
    {"DAV:", "version-name", ELEM_version_name},
    {"DAV:", "creator-displayname", ELEM_creator_displayname},
    {"DAV:", "getcontentlength", ELEM_getcontentlength},
    {"DAV:", "getlastmodified", ELEM_getlastmodified},
};

/* We do not validate at this point */
/*
static int validate_report_elements(void *userdata,
				    ne_xml_elmid parent, ne_xml_elmid child)
{
    return NE_XML_VALID;
}
*/

/* Set xml parser error */
static void set_xml_error(report_ctx * sctx, const char *format, ...)
{
    va_list ap;
    char buf[512];

    va_start(ap, format);
    ne_vsnprintf(buf, sizeof buf, format, ap);
    va_end(ap);

    ne_set_error(session.sess, "%s", buf);
    sctx->err_code = NE_ERROR;
}

static int start_element(void *userdata, int parent,
			 const char *nspace, 
			 const char *name, 
			 const char **atts)
{
    report_ctx *rctx = (report_ctx *) userdata;
    int state = ne_xml_mapid(report_elements, 
			     NE_XML_MAPLEN(report_elements), nspace, name);

    /* Error occured, ignore remain part */
    if (rctx->err_code != NE_OK)
	return rctx->err_code;

    ne_buffer_clear(rctx->cdata);

    switch (state) {

    case ELEM_response:	/* Start of new response */
	rctx->curr = ne_calloc(sizeof(report_res));
	rctx->result_num++;
	break;
	
    case ELEM_prop:		/* Start of prop */
	if (rctx->curr == NULL) {
	    set_xml_error(rctx, "XML : <%s> is in the wrong place",
			  name);
	    break;
	}
	rctx->start_prop = 1;
	break;
    
    case ELEM_propstat:	/* expecting props */
    case ELEM_href:		/* href */
    case ELEM_ignore:
    default:
	break;
    }

    return state;
}

#define REPORT_CP_ELEM(rctx, curr, name, desc, src) \
do { \
      if ((curr) == NULL) \
         set_xml_error((rctx),  "XML : </%s> is in the wrong place", (name));\
      else if (src)\
         (desc) = ne_strdup(src);\
} while (0)


static int cdata_report(void *userdata, int state, const char *buf, size_t len)
{
    report_ctx *rctx = (report_ctx *) userdata;
    ne_buffer_append(rctx->cdata, buf, len);
    return 0;
}


static int 
end_element(void *userdata, int state, const char *nspace, const char *name)
{
    report_ctx *rctx = (report_ctx *) userdata;
    const char *cdata = rctx->cdata->data;
 
    /* Error occured, ignore remain part */
    if (rctx->err_code != NE_OK)
	return rctx->err_code;
    
    switch (state) {
    case ELEM_response:	/* End of new response */
	/* Nothing to add */
	if (rctx->curr == NULL) {
	    set_xml_error(rctx, "XML : </%s> is in the wrong place",
			  name);
	    break;
	}
	
	/* No HREF */
	if (rctx->curr->href == NULL) {
	    set_xml_error(rctx, "XML : No href info in the <%s>...</%s>",
			  name, name);
	    break;
	}
	/* make link */
	rctx->curr->next = rctx->root;
	rctx->root = rctx->curr;
	rctx->curr = NULL;
	break;
	
    case ELEM_href:		/* href */
	REPORT_CP_ELEM(rctx, rctx->curr, name, rctx->curr->href, cdata);
	break;
	
    case ELEM_version_name:
	REPORT_CP_ELEM(rctx, rctx->curr, name,
		       rctx->curr->version_name, cdata);
	break;
	
    case ELEM_creator_displayname:
	REPORT_CP_ELEM(rctx, rctx->curr, name,
		       rctx->curr->creator_displayname, cdata);
	break;

    case ELEM_getcontentlength:
	REPORT_CP_ELEM(rctx, rctx->curr, name,
		       rctx->curr->getcontentlength, cdata);
	break;
	
    case ELEM_getlastmodified:
	REPORT_CP_ELEM(rctx, rctx->curr, name,
		       rctx->curr->getlastmodified, cdata);
	break;
	
    case ELEM_prop:		/* Start of prop */
	if (rctx->curr == NULL)
	    set_xml_error(rctx, "XML : </%s> is in the wrong place",
			  name);
	else			/* stop to props */
	    rctx->start_prop = 0;
	break;
	
    case ELEM_ignore:
    case ELEM_propstat:	/* expecting props */
    default:
	break;
    }

    return NE_OK;
}


static void report_ctx_destroy(report_ctx * sctx)
{
    report_res *res, *res_free;

    ne_buffer_destroy(sctx->cdata);

    for (res = sctx->root; res;) {
	NE_FREE(res->href);
	
	/* live props */
	NE_FREE(res->version_name);
	NE_FREE(res->creator_displayname);
	NE_FREE(res->getcontentlength);
	NE_FREE(res->getlastmodified);

	res_free = res;
	res = res->next;
	NE_FREE(res_free);
    }
}

/* Give UI feedback for request 'req', which got dispatch return code
 * 'ret'. */
static void req_result(ne_request *req, int ret)
{
    if (ret != NE_OK) {
        out_result(ret);
    } else if (ne_get_status(req)->klass != 2) {
        out_result(NE_ERROR);
    } else {
        out_success();
    }
}

static void simple_request(const char *remote, const char *verb, 
                           const char *method)
{
    char *real_remote;
    ne_request *req;
    int ret;
    
    if (remote != NULL) {
	real_remote = resolve_path(session.uri.path, remote, true);
    } else {
	real_remote = ne_strdup(session.uri.path);
    }

    out_start(verb, remote);

    req = ne_request_create(session.sess, method, real_remote);

    ne_lock_using_resource(req, real_remote, 0);

    ret = ne_request_dispatch(req);
    
    req_result(req, ret);

    ne_request_destroy(req);
    free(real_remote);
}

void execute_version(const char *remote)
{
    simple_request(remote, _("Versioning"), "VERSION-CONTROL");
}

void execute_checkin(const char *remote)
{
    simple_request(remote, _("Checking in"), "CHECKIN");
}

void execute_checkout(const char *remote)
{
    simple_request(remote, _("Checking out"), "CHECKOUT");
}

void execute_uncheckout(const char *remote)
{
    simple_request(remote, _("Cancelling check out of"), "UNCHECKOUT");
}

/* displays report results */
static int display_report_results(report_ctx * rctx)
{
    report_res *res;
    
    if (rctx->err_code) {
	return rctx->err_code;
    }

    printf(_(" %d version%s in history:\n"), rctx->result_num,
           rctx->result_num==1?"":"s");

    for (res = rctx->root; res; res = res->next) {
	long modtime = res->getlastmodified ?
	  ne_httpdate_parse(res->getlastmodified) : 0;
	int size = res->getcontentlength ? atol(res->getcontentlength) : 0;
	
	printf("%-40s %10d  %s <%s>\n", res->href,
	       size, format_time(modtime), res->version_name);
    }

    return rctx->err_code;
}

static int do_report(const char *real_remote, 
		     report_ctx *rctx)
{
    int ret;
    ne_request *req;
    ne_xml_parser *report_parser;
    
    /* create/prep the request */
    if ((req = ne_request_create(session.sess, "REPORT", real_remote)) == NULL)
	return NE_ERROR;
    
    /* Plug our XML parser */
    report_parser = ne_xml_create();
    ne_xml_push_handler(report_parser, start_element, 
			cdata_report, 
			end_element,
			rctx);
    
    ne_add_request_header(req, "Content-Type", NE_XML_MEDIA_TYPE);
    
    ne_add_response_body_reader(req, ne_accept_2xx, ne_xml_parse_v,
				report_parser);
    /* Set body */
    ne_set_request_body_buffer(req, report_body, strlen(report_body));
    
    /* run the request, see what comes back. */
    ret = ne_request_dispatch(req);
    
    if (ne_get_status(req)->klass != 2) 
	ret = ne_get_status(req)->code;

    ne_request_destroy(req);

    return ret;
}


void execute_history(const char *remote)
{
    int ret;
    char *real_remote;
    report_ctx *rctx = ne_calloc(sizeof(report_ctx));
    rctx->cdata = ne_buffer_create();
    
    if (remote != NULL) {
	real_remote = resolve_path(session.uri.path, remote, false);
    } else {
	real_remote = ne_strdup(session.uri.path);
    }
    out_start(_("Version history of"), real_remote);
    
    /* Run search */
    ret = do_report(real_remote, rctx);
    if (ret != NE_OK) {
	/* Print out error message */
	out_result(ret);
    } else {
        /* show report result */
	display_report_results(rctx);
    }

    free(real_remote);
    report_ctx_destroy(rctx);
}


void execute_label(const char *remote, const char *act, const char *value)
{
    int ret;
    char *real_remote;
    ne_request *req;
    ne_buffer *label_body;

    if (strcasecmp(act, "add") && strcasecmp(act, "remove") &&
        strcasecmp(act, "set")) {
        printf(_("Invalid action `%s' given.\n"), act);
        return;
    }

    if (remote != NULL) {
	real_remote = resolve_path(session.uri.path, remote, true);
    } else {
	real_remote = ne_strdup(session.uri.path);
    }

    out_start(_("Labelling"), real_remote);

    /* Create Label Body */
    label_body = ne_buffer_create();
    
    /* Create the request body */
    ne_buffer_zappend(label_body, 
		      "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
		      "<D:label xmlns:D=\"DAV:\">\n");
    
    /* Adction */
    ne_buffer_concat(label_body, "<D:", act, "><D:label-name>", value, 
		     "</D:label-name></D:", act, ">", NULL);
    ne_buffer_zappend(label_body,
		      "</D:label>\n");
    
    /* create/prep the request */
    req = ne_request_create(session.sess, "LABEL", real_remote);
    
    ne_add_request_header(req, "Content-Type", NE_XML_MEDIA_TYPE);
    
    /* Set body */
    ne_set_request_body_buffer(req, label_body->data, 
			       ne_buffer_size(label_body));
    
    /* run the request, see what comes back. */
    ret = ne_request_dispatch(req);

    /* Print out status */
    req_result(req, ret);
    
    ne_buffer_destroy(label_body);
    ne_request_destroy(req);
    free(real_remote);
}

static const ne_propname vcr_props[] = {
    { "DAV:", "checked-in" },
    { "DAV:", "checked-out" },
    { NULL }
};

static void vcr_results(void *userdata, const char *uri,
                        const ne_prop_result_set *set)
{
    const char *checkin, *checkout;
    int *isvcr = userdata;
    
    checkin = ne_propset_value(set, &vcr_props[0]);
    checkout = ne_propset_value(set, &vcr_props[1]);
    
    /* is vcr */
    if (checkin) {
	*isvcr = 1;
    } else if (checkout) {
	*isvcr = 2;
    }
}

int is_vcr(const char *uri) 
{ 
    int ret, vcr = 0;
    ne_propfind_handler *pfh = ne_propfind_create(session.sess, uri, NE_DEPTH_ZERO);
    ret = ne_propfind_named(pfh, vcr_props, vcr_results, &vcr);
    ne_propfind_destroy(pfh);

    if (ret != NE_OK)
	return 0;

    return vcr;
}
