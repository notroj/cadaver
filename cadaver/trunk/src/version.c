/* 
   'version' for cadaver
   Copyright (C) 2002, GRASE Lab, UCSC <grase@cse.ucsc.edu>, 
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
}
report_ctx;
    
enum
{
    ELEM_multistatus = NE_ELM_207_UNUSED,
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

    ELEM_ignore,
};


static const struct ne_xml_elm report_elements[] = {
    {"DAV:", "multistatus", ELEM_multistatus, 0},
    {"DAV:", "response", ELEM_response, 0},
    {"DAV:", "href", ELEM_href, NE_XML_CDATA},
    {"DAV:", "propstat", ELEM_propstat, 0},
    {"DAV:", "prop", ELEM_prop, 0},
    {"DAV:", "status", ELEM_status, NE_XML_CDATA},

    /* Live props */
    {"DAV:", "version-name", ELEM_version_name, NE_XML_CDATA},
    {"DAV:", "creator-displayname", ELEM_creator_displayname, NE_XML_CDATA},
    {"DAV:", "getcontentlength", ELEM_getcontentlength, NE_XML_CDATA},
    {"DAV:", "getlastmodified", ELEM_getlastmodified, NE_XML_CDATA},
    
    /* It deals all unknown elements */
    {"", "", NE_ELM_unknown, NE_XML_COLLECT},
    {NULL}
};

/* We do not validate at this piint */
static int validate_report_elements(void *userdata,
				    ne_xml_elmid parent, ne_xml_elmid child)
{
    return NE_XML_VALID;
}

/* Set xml parser error */
static void set_xml_error(report_ctx * sctx, const char *format, ...)
{
    sctx->err_code = NE_ERROR;
    ne_set_error(session, format);
}

static int start_element(void *userdata, const struct ne_xml_elm *elm,
			 const char **atts)
{
    report_ctx *rctx = (report_ctx *) userdata;

    /* Error occured, ignore remain part */
    if (rctx->err_code != NE_OK)
	return rctx->err_code;

    switch (elm->id) {

    case ELEM_response:	/* Start of new response */
	rctx->curr = ne_calloc(sizeof(report_res));
	rctx->result_num++;
	break;
	
    case ELEM_prop:		/* Start of prop */
	if (rctx->curr == NULL) {
	    set_xml_error(rctx, "XML : <%s> is in the wrong place",
			  elm->name);
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

    return NE_XML_VALID;
}

#define REPORT_CP_ELEM(rctx, curr, name, desc, src) \
do { \
      if ((curr) == NULL) \
         set_xml_error((rctx),  "XML : </%s> is in the wrong place", (name));\
      else \
         (desc) = ne_strdup(src);\
} while (0)


static int end_element(void *userdata, const struct ne_xml_elm *elm,
		       const char *cdata)
{
    report_ctx *rctx = (report_ctx *) userdata;
    
    /* Error occured, ignore remain part */
    if (rctx->err_code != NE_OK)
	return rctx->err_code;
    
    switch (elm->id) {
    case ELEM_response:	/* End of new response */
	/* Nothing to add */
	if (rctx->curr == NULL) {
	    set_xml_error(rctx, "XML : </%s> is in the wrong place",
			  elm->name);
	    break;
	}
	
	/* No HREF */
	if (rctx->curr->href == NULL) {
	    set_xml_error(rctx, "XML : No href info in the <%s>...</%s>",
			  elm->name, elm->name);
	    break;
	}
	/* make link */
	rctx->curr->next = rctx->root;
	rctx->root = rctx->curr;
	rctx->curr = NULL;
	break;
	
    case ELEM_href:		/* href */
	REPORT_CP_ELEM(rctx, rctx->curr, elm->name, rctx->curr->href, cdata);
	break;
	
    case ELEM_version_name:
	REPORT_CP_ELEM(rctx, rctx->curr, elm->name,
		       rctx->curr->version_name, cdata);
	break;
	
    case ELEM_creator_displayname:
	REPORT_CP_ELEM(rctx, rctx->curr, elm->name,
		       rctx->curr->creator_displayname, cdata);
	break;

    case ELEM_getcontentlength:
	REPORT_CP_ELEM(rctx, rctx->curr, elm->name,
		       rctx->curr->getcontentlength, cdata);
	break;
	
    case ELEM_getlastmodified:
	REPORT_CP_ELEM(rctx, rctx->curr, elm->name,
		       rctx->curr->getlastmodified, cdata);
	break;
	
    case ELEM_prop:		/* Start of prop */
	if (rctx->curr == NULL)
	    set_xml_error(rctx, "XML : </%s> is in the wrong place",
			  elm->name);
	else			/* stop to props */
	    rctx->start_prop = 0;
	break;
	
    case ELEM_ignore:
    case ELEM_propstat:	/* expecting props */
    default:
	break;
    }

    return NE_XML_VALID;
}


static void report_ctx_destroy(report_ctx * sctx)
{
    report_res *res, *res_free;
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
	real_remote = resolve_path(path, remote, true);
    } else {
	real_remote = ne_strdup(path);
    }

    out_start(verb, real_remote);

    req = ne_request_create(session, method, real_remote);

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
    if ((req = ne_request_create(session, "REPORT", real_remote)) == NULL)
	return NE_ERROR;
    
    /* Plug our XML parser */
    report_parser = ne_xml_create();
    ne_xml_push_handler(report_parser, report_elements,
			validate_report_elements, start_element, end_element,
			rctx);
    
    ne_add_request_header(req, "Content-Type", NE_XML_MEDIA_TYPE);
    
    ne_add_response_body_reader(req, ne_accept_2xx, ne_xml_parse_v,
				report_parser);
    
    /* Set body */
    ne_set_request_body_buffer(req, report_body, strlen(report_body));
    
    /* run the request, see what comes back. */
    ret = ne_request_dispatch(req);
    
    ne_request_destroy(req);

    return ret;
}


void execute_history(const char *remote)
{
    int ret;
    char *real_remote;
    report_ctx *rctx = ne_calloc(sizeof(report_ctx));
        
    if (remote != NULL) {
	real_remote = resolve_path(path, remote, false);
    } else {
	real_remote = ne_strdup(path);
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
	real_remote = resolve_path(path, remote, true);
    } else {
	real_remote = ne_strdup(path);
    }

    out_start(_("Labelling"), real_remote);

    /* Create Label Body */
    label_body = ne_buffer_create();
    
    /* Create the request body */
    ne_buffer_zappend(label_body, 
		      "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" EOL
		      "<D:label xmlns:D=\"DAV:\">"
		      EOL);
    
    /* Adction */
    ne_buffer_concat(label_body, "<D:", act, "><D:label-name>", value, 
		     "</D:label-name></D:", act, ">", NULL);
    ne_buffer_zappend(label_body,
		      "</D:label>" EOL);
    
    /* create/prep the request */
    req = ne_request_create(session, "LABEL", real_remote);
    
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
