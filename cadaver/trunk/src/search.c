/* 
   'search' for cadaver
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

#include <ne_basic.h>
#include <ne_request.h>
#include <ne_props.h>
#include <ne_uri.h>
#include <ne_alloc.h>
#include <ne_dates.h>

#include "i18n.h"
#include "commands.h"
#include "cadaver.h"
#include "basename.h"
#include "options.h"
#include "utils.h"

/* From ne_props.c
 * Need to move util.c or something? */
#define NSPACE(x) ((x) ? (x) : "")

/*
 * The Macro definations that 'wordtype' might be.
 */
#define QUOT            101	/*''' */
#define COMMA           102	/*',' */
#define LPAR            103	/*'(' */
#define RPAR            104	/*')' */
#define EQ              105	/*'=' */
#define LE              106	/*'<=' */
#define LT              107	/*'<' */
#define GE              108	/*'>=' */
#define GT              109	/*'>' */
#define NEQ             110	/*'<>' */
#define IDEN            111	/*Integer */
#define INTEGER         112	/*identifier */
#define UNKNOWN         113	/*unknown charactor */
#define ENDBUF          114	/*End of the buffer */

#define WORDLEN 256		/*Max length of a identifier(token) in the search command */

/* Dead prop */
typedef struct dead_prop
{
    char *name;
    char *nspace;
    char *value;
    struct dead_prop *next;
}
dead_prop;

typedef struct search_res
{
    char *href;
    /* live props */
    char *creationdate;
    char *displayname;
    char *getcontentlanguage;
    char *getcontentlength;
    char *getcontenttype;
    char *getetag;
    char *getlastmodified;
    char *lockdiscovery;
    char *resourcetype;
    char *source;
    char *supportedlock;
    char *collection;

    dead_prop *root;
    dead_prop *curr;
    int dead_prop_num;

    struct search_res *next;
}
search_res;

/* Search XML parser context */
typedef struct
{
    search_res *root;
    search_res *curr;
    int result_num;
    int start_prop;
    int err_code;
}
search_ctx;

enum
{
    ELEM_multistatus = NE_ELM_207_UNUSED,
    ELEM_response,
    ELEM_href,
    ELEM_prop,
    ELEM_propstat,
    ELEM_status,
    ELEM_responsedescription,

    /* props from RFC 2518 , 23 Appendices 23.1 */
    ELEM_creationdate,
    ELEM_displayname,
    ELEM_getcontentlanguage,
    ELEM_getcontentlength,
    ELEM_getcontenttype,
    ELEM_getetag,
    ELEM_getlastmodified,
    ELEM_lockdiscovery,
    ELEM_resourcetype,
    ELEM_source,
    ELEM_supportedlock,
    ELEM_collection,

    ELEM_ignore,
};

static const struct ne_xml_elm search_elements[] = {
    {"DAV:", "multistatus", ELEM_multistatus, 0},
    {"DAV:", "response", ELEM_response, 0},
    {"DAV:", "responsedescription", ELEM_responsedescription,
     NE_XML_CDATA},
    {"DAV:", "href", ELEM_href, NE_XML_CDATA},
    {"DAV:", "propstat", ELEM_propstat, 0},
    {"DAV:", "prop", ELEM_prop, 0},
    {"DAV:", "status", ELEM_status, NE_XML_CDATA},

    /* Live props */
    {"DAV:", "creationdate", ELEM_creationdate, NE_XML_CDATA},
    {"DAV:", "displayname", ELEM_displayname, NE_XML_CDATA},
    {"DAV:", "getcontentlanguage", ELEM_getcontentlanguage, NE_XML_CDATA},
    {"DAV:", "getcontentlength", ELEM_getcontentlength, NE_XML_CDATA},
    {"DAV:", "getcontenttype", ELEM_getcontenttype, NE_XML_CDATA},
    {"DAV:", "getetag", ELEM_getetag, NE_XML_CDATA},
    {"DAV:", "getlastmodified", ELEM_getlastmodified, NE_XML_CDATA},
    {"DAV:", "lockdiscovery", ELEM_lockdiscovery, NE_XML_CDATA},
    {"DAV:", "resourcetype", ELEM_resourcetype, NE_XML_CDATA},
    {"DAV:", "source", ELEM_source, NE_XML_CDATA},
    {"DAV:", "supportedlock", ELEM_supportedlock, NE_XML_CDATA},
    {"DAV:", "collection", ELEM_collection, NE_XML_CDATA},

    /* Ignore it for now */
    {"DAV:", "lockentry", ELEM_ignore, NE_XML_CDATA},
    {"DAV:", "lockscope", ELEM_ignore, NE_XML_CDATA},
    {"DAV:", "locktype", ELEM_ignore, NE_XML_CDATA},
    {"DAV:", "exclusive", ELEM_ignore, NE_XML_CDATA},
    {"DAV:", "shared", ELEM_ignore, NE_XML_CDATA},
    {"DAV:", "read", ELEM_ignore, NE_XML_CDATA},
    {"DAV:", "write", ELEM_ignore, NE_XML_CDATA},

    /* It deals all unknown elements */
    {"", "", NE_ELM_unknown, NE_XML_COLLECT},
    {NULL}
};

/* 
 * Basic search parser functions
 * return NE_OK or error_no
 * basic_search must be allcated before the function call.
 */
static int search_select_gen(const ne_propname * props,
			     ne_buffer * basic_search);
static int search_from_gen(const char *href, const int depth,
			   ne_buffer * basic_search);
static int search_where_gen(const char *query, ne_buffer * basic_search);
static int search_orderby_gen(const ne_propname * asc,
			      const ne_propname * des,
			      ne_buffer * basic_search);

/*
 * Static functions for where condition parser
 */
static int read_aword(char **string_parsed, char *word_fetched);
static int first_word_equal(const char *string_parsed,
			    const char *word_to_compare);
static int match_fetch(char **string_parsed, const char *str_expected);
static int search_condition(char **string_parsed, ne_buffer * result_buf);
static int boolean_term(char **strparsed, ne_buffer * result_buf);
static int boolean_factor(char **string_parsed, ne_buffer * result_buf);
static int boolean_primary(char **string_parsed, ne_buffer * result_buf);
static int operator_translate(const char *operator, char *XML_operator);
static int predicate(char **string_parsed, ne_buffer * result_buf);
static int contains_predicate(char **string_parsed, ne_buffer * result_buf);
static int quoted_string(char **string_parsed, ne_buffer * result_buf);
static int comparison_value(char **string_parsed, ne_buffer * result_buf);
static int word_string(char **string_parsed, ne_buffer * result_buf);

/* We do not validate at this piint */
static int validate_search_elements(void *userdata,
				    ne_xml_elmid parent, ne_xml_elmid child)
{
    return NE_XML_VALID;
}

/* Set xml parser error */
static void set_xml_error(search_ctx * sctx, const char *format, ...)
{
    sctx->err_code = NE_ERROR;
    ne_set_error(session, format);
}

static int start_element(void *userdata, const struct ne_xml_elm *elm,
			 const char **atts)
{
    search_ctx *sctx = (search_ctx *) userdata;

    /* Error occured, ignore remain part */
    if (sctx->err_code != NE_OK)
	return sctx->err_code;

    switch (elm->id) {
    case ELEM_ignore:
	break;

    case ELEM_response:	/* Start of new response */
	sctx->curr = ne_calloc(sizeof(search_res));
	sctx->result_num++;
	break;

    case ELEM_href:		/* href */
	break;

    case ELEM_propstat:	/* expecting props */
	break;

    case ELEM_prop:		/* Start of prop */
	if (sctx->curr == NULL) {
	    set_xml_error(sctx, "XML : <%s> is in the wrong place",
			  elm->name);
	    break;
	}
	sctx->start_prop = 1;
	break;
    default:
	if (get_bool_option(opt_searchall) &&	/* check searchall option */
	    sctx->curr && sctx->start_prop == 1) {	/* It's prop */
	    search_res *res = sctx->curr;
	    res->dead_prop_num++;
	    res->curr = (dead_prop *) ne_calloc(sizeof(dead_prop));
	    res->curr->name = ne_strdup(elm->name);
	    res->curr->nspace = ne_strdup(elm->nspace);
	}
	break;
    }

    return NE_XML_VALID;
}

#define SEARCH_CP_ELEM(sctx, curr, name, desc, src) \
do { \
      if ((curr) == NULL) \
         set_xml_error((sctx),  "XML : </%s> is in the wrong place", (name));\
      else \
         (desc) = ne_strdup(src);\
} while (0)


static int end_element(void *userdata, const struct ne_xml_elm *elm,
		       const char *cdata)
{
    search_ctx *sctx = (search_ctx *) userdata;

    /* Error occured, ignore remain part */
    if (sctx->err_code != NE_OK)
	return sctx->err_code;

    switch (elm->id) {
    case ELEM_ignore:
	break;

    case ELEM_response:	/* End of new response */
	/* Nothing to add */
	if (sctx->curr == NULL) {
	    set_xml_error(sctx, "XML : </%s> is in the wrong place",
			  elm->name);
	    break;
	}

	/* No HREF */
	if (sctx->curr->href == NULL) {
	    set_xml_error(sctx, "XML : No href info in the <%s>...</%s>",
			  elm->name, elm->name);
	    break;
	}
	/* make link */
	sctx->curr->next = sctx->root;
	sctx->root = sctx->curr;
	sctx->curr = NULL;
	break;

    case ELEM_href:		/* href */
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name, sctx->curr->href, cdata);
	break;

	/* live props */
    case ELEM_creationdate:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->creationdate, cdata);
	break;

    case ELEM_displayname:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->displayname, cdata);
	break;

    case ELEM_getcontentlanguage:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->getcontentlanguage, cdata);
	break;

    case ELEM_getcontentlength:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->getcontentlength, cdata);
	break;

    case ELEM_getcontenttype:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->getcontenttype, cdata);
	break;

    case ELEM_getetag:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->getetag, cdata);
	break;

    case ELEM_getlastmodified:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->getlastmodified, cdata);
	break;

    case ELEM_lockdiscovery:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->lockdiscovery, cdata);
	break;

    case ELEM_resourcetype:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->resourcetype, cdata);
	break;

    case ELEM_source:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->source, cdata);
	break;

    case ELEM_supportedlock:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->supportedlock, cdata);
	break;

    case ELEM_collection:
	SEARCH_CP_ELEM(sctx, sctx->curr, elm->name,
		       sctx->curr->collection, cdata);
	break;

    case ELEM_propstat:	/* expecting props */
	break;

    case ELEM_prop:		/* Start of prop */
	if (sctx->curr == NULL)
	    set_xml_error(sctx, "XML : </%s> is in the wrong place",
			  elm->name);
	else			/* stop to props */
	    sctx->start_prop = 0;
	break;

    default:
	if (get_bool_option(opt_searchall) &&	/* check searchall option */
	    sctx->curr && sctx->start_prop == 1) {	/* It's dead prop */
	    search_res *res = sctx->curr;
	    res->curr->value = ne_strdup(cdata);
	    res->curr->next = res->root;
	    res->root = res->curr;
	    res->curr = NULL;
	}
	break;
    }

    return NE_XML_VALID;
}

/* displays search results */
static int display_results(search_ctx * sctx)
{
    search_res *res;
    dead_prop *dprop;

    if (sctx->err_code) {
	return sctx->err_code;
    }

    printf("Found %d results\n\n", sctx->result_num);
    for (res = sctx->root; res; res = res->next) {
	long modtime = res->getlastmodified ?
	    ne_httpdate_parse(res->getlastmodified) : 0;
	int size = res->getcontentlength ? atol(res->getcontentlength) : 0;
	char exec_char = ' ';

	printf("%-40s%c %10d  %s <%.10s>\n", res->href, exec_char,
	       size, format_time(modtime), res->getcontenttype);

	for (dprop = res->root;
	     get_bool_option(opt_searchall) && dprop; dprop = dprop->next)
	    printf("\t-  %s:%s = %s\n",	/* better way to show ? */
		   dprop->nspace, dprop->name, dprop->value);
    }

    return sctx->err_code;
}

static void search_ctx_destroy(search_ctx * sctx)
{
    search_res *res, *res_free;
    dead_prop *dprop, *dprop_free;

    for (res = sctx->root; res;) {
	NE_FREE(res->href);
	/* live props */
	NE_FREE(res->creationdate);
	NE_FREE(res->displayname);
	NE_FREE(res->getcontentlanguage);
	NE_FREE(res->getcontentlength);
	NE_FREE(res->getcontenttype);
	NE_FREE(res->getetag);
	NE_FREE(res->getlastmodified);
	NE_FREE(res->lockdiscovery);
	NE_FREE(res->resourcetype);
	NE_FREE(res->source);
	NE_FREE(res->supportedlock);
	NE_FREE(res->collection);

	for (dprop = res->root; dprop;) {
	    NE_FREE(dprop->nspace);
	    NE_FREE(dprop->name);
	    NE_FREE(dprop->value);

	    dprop_free = dprop;
	    dprop = dprop->next;
	    NE_FREE(dprop_free);
	}
	res_free = res;
	res = res->next;
	NE_FREE(res_free);
    }
}

/* create propname struct from searchorder setting */
static ne_propname *order_props_create(const char *str)
{
    int n;
    char *buf;
    char *tok;
    char *delm = " \t\n\r";
    int num_props = 0;
    ne_propname *props;

    /* No props */
    if (str == NULL)
	return NULL;

    buf = ne_strdup(str);
    if (strtok(buf, delm)) {
	num_props = 1;
	while (strtok(NULL, delm))
	    num_props++;
    }

    /* No props */
    if (num_props == 0)
	return NULL;

    /* One more for last NULL */
    props = ne_calloc(sizeof(ne_propname) * (num_props + 1));

    /* Set first token */
    NE_FREE(buf);
    buf = ne_strdup(str);
    tok = strtok(buf, delm);

    /* Other idea? */
    props[0].nspace = ne_strdup("DAV:");
    props[0].name = ne_strdup(tok);

    for (n = 1; (tok = strtok(NULL, delm)); n++) {
	props[n].nspace = ne_strdup("DAV:");
	props[n].name = ne_strdup(tok);
    }

    NE_FREE(buf);
    return props;
}

static void order_props_destroy(ne_propname * props)
{
    int n;

    if (props == NULL)
	return;

    for (n = 0; props[n].name != NULL; n++) {
	NE_FREE((char *) props[n].name);
	NE_FREE((char *) props[n].nspace);
    }

    NE_FREE(props);
}

/* Run search and get the data to sctx */
static int run_search(ne_session * sess, const char *uri,
		      int depth, ne_buffer * query, search_ctx * sctx)
{
    int ret;
    ne_request *req;
    ne_buffer *basic_search = ne_buffer_create();
    ne_xml_parser *search_parser;
    const char *searchorder = (const char *) get_option(opt_searchorder);
    const char *searchdorder = (const char *) get_option(opt_searchdorder);
    ne_propname *asc = order_props_create(searchorder);
    ne_propname *des = order_props_create(searchdorder);

    /* create/prep the request */
    if ((req = ne_request_create(sess, "SEARCH", uri)) == NULL)
	return NE_ERROR;

    /* Create the request body */
    ne_buffer_zappend(basic_search,
		      "<?xml version=\"1.0\" encoding=\"utf-8\" ?>" EOL
		      "<D:searchrequest xmlns:D=\"DAV:\"><D:basicsearch>"
		      EOL);

    if (search_select_gen(NULL, basic_search) != NE_OK)
	return NE_ERROR;

    if (search_from_gen(uri, depth, basic_search) != NE_OK)
	return NE_ERROR;

    if (search_where_gen(query->data, basic_search) != NE_OK)
	return NE_ERROR;

    if (search_orderby_gen(asc, des, basic_search) != NE_OK)
	return NE_ERROR;

    ne_buffer_zappend(basic_search, "</D:basicsearch></D:searchrequest>" EOL);

    ne_set_request_body_buffer(req, basic_search->data,
			       ne_buffer_size(basic_search));

    /* Plug our XML parser */
    search_parser = ne_xml_create();
    ne_xml_push_handler(search_parser, search_elements,
			validate_search_elements, start_element, end_element,
			sctx);

    ne_add_request_header(req, "Content-Type", NE_XML_MEDIA_TYPE);
    ne_add_depth_header(req, depth);

    ne_add_response_body_reader(req, ne_accept_207, ne_xml_parse_v,
				search_parser);

    /* run the request, see what comes back. */
    if ((ret = ne_request_dispatch(req)) != NE_OK)
	return ret;

    /* Check Errors from XML parser */
    if (sctx->err_code != NE_OK)
	return NE_ERROR;

    /* Get response code */
    if (ne_get_status(req)->code != 207)
	return NE_ERROR;

    /* destroy request, parse, and etc */
    order_props_destroy(asc);
    order_props_destroy(des);

    ne_buffer_destroy(basic_search);
    ne_request_destroy(req);
    ne_xml_destroy(search_parser);

    return NE_OK;
}

/* Main execute routine for search */
void execute_search(int count, const char **args)
{
    int ret;
    const char **pnt;
    search_ctx *sctx = ne_calloc(sizeof(search_ctx));
    ne_buffer *query = ne_buffer_create();

    /* default is success */
    sctx->err_code = NE_OK;

    for (pnt = args; *pnt != NULL; pnt++) {
	/* Need quota */
	if (strchr(*pnt, ' ') || strchr(*pnt, '\t'))
	    ne_buffer_concat(query, "'", *pnt, "' ", NULL);
	else
	    ne_buffer_concat(query, *pnt, " ", NULL);
    }

    printf(_("Using query: "));
    printf("%s, ", query->data);

    /* Run search and get data to sctx */
    ret = run_search(session, path, searchdepth, query, sctx);
    if (ret == NE_OK) {
	display_results(sctx);
    }

    out_result(ret);

    search_ctx_destroy(sctx);
    ne_buffer_destroy(query);
}

/* Generate select part of the query. allprop if the props arg is NULL. */
static int search_select_gen(const ne_propname * props,
			     ne_buffer * basic_search)
{
    int n;

    if (!basic_search) {
	ne_set_error(session, "select_gen: no buffer");
	return NE_ERROR;
    }

    if (props == NULL) {
	ne_buffer_zappend(basic_search,
			  "<D:select><D:allprop/></D:select>" EOL);
	return NE_OK;
    }

    ne_buffer_zappend(basic_search, "<D:select><D:prop>" EOL);

    for (n = 0; props[n].name != NULL; n++) {
	ne_buffer_concat(basic_search, "<", props[n].name, " xmlns=\"",
			 NSPACE(props[n].nspace), "\"/>" EOL, NULL);
    }

    ne_buffer_zappend(basic_search, "</D:prop></D:select>" EOL);

    return NE_OK;
}

static int search_from_gen(const char *href, const int depth,
			   ne_buffer * basic_search)
{
    const char *depth_str;

    if (!basic_search || !href) {
	ne_set_error(session, "from_gen: no buffer or no href");
	return NE_ERROR;
    }

    switch (depth) {
    case NE_DEPTH_ONE:
	depth_str = "1";
	break;
    case NE_DEPTH_ZERO:
	depth_str = "0";
	break;
    default:
	depth_str = "infinity";
	break;
    }

    ne_buffer_concat(basic_search, "<D:from><D:scope><D:href>",
		     href, "</D:href><D:depth>", depth_str,
		     "</D:depth></D:scope></D:from>" EOL, NULL);

    return NE_OK;
}

/* Parse a searchquery. It will call the search_condition() function and
 * check the ending status. If the ending status is not ENDBUF, then there will
 * be a syntax error.
 * The parsing result will be saved in 'result_buf'.
 *
 * Returns:
 * NE_OK: success
 * NE_ERROR: syntax error
 * */
static int search_where_gen(const char *condition_str,
			    ne_buffer * basic_search)
{
    char identifier[WORDLEN + 1] = "";
    char *string_parsed = ne_strdup(condition_str);
    char *ptr_backup = string_parsed;
    /* The buffer storing the parsing result of search condition */
    ne_buffer *result_buf;

    if (!basic_search || !condition_str) {
	ne_set_error(session, "where_gen: no buffer or no query");
	return NE_ERROR;
    }

    result_buf = ne_buffer_create();

    /* Fill boby from <D:where> to </D:where> */
    ne_buffer_zappend(basic_search, "<D:where>" EOL);

    if (search_condition(&string_parsed, result_buf) == NE_ERROR) {
	NE_FREE(ptr_backup);
	ne_buffer_destroy(result_buf);
	return NE_ERROR;	/*Parsing error */
    }

    /*The ending of a condition must be an ENDBUF */
    if (read_aword(&string_parsed, identifier) != ENDBUF) {
	ne_set_error(session, "Syntax error in the search condition.");
	NE_FREE(ptr_backup);
	ne_buffer_destroy(result_buf);
	return NE_ERROR;
    }

    /*Append the parsing result of the search condition */
    ne_buffer_zappend(basic_search, result_buf->data);

    ne_buffer_zappend(basic_search, "</D:where>" EOL);

    NE_FREE(ptr_backup);
    ne_buffer_destroy(result_buf);

    return NE_OK;
}				/*End of ne_search_where_gen */

static int search_orderby_gen(const ne_propname * asc,
			      const ne_propname * des,
			      ne_buffer * basic_search)
{
    int n;

    if (!basic_search) {
	ne_set_error(session, "orderby_gen: no buffer or no query");
	return NE_ERROR;
    }

    /* No order information */
    if (asc == NULL && des == NULL)
	return NE_OK;

    ne_buffer_zappend(basic_search, "<D:orderby>" EOL);

    for (n = 0; asc && asc[n].name != NULL; n++) {
	ne_buffer_zappend(basic_search, "<D:order><D:prop>" EOL);
	ne_buffer_concat(basic_search, "<", asc[n].name, " xmlns=\"",
			 NSPACE(asc[n].nspace), "\"/>" EOL, NULL);
	ne_buffer_zappend(basic_search,
			  "</D:prop><D:ascending/></D:order>" EOL);
    }

    for (n = 0; des && des[n].name != NULL; n++) {
	ne_buffer_zappend(basic_search, "<D:order><D:prop>" EOL);
	ne_buffer_concat(basic_search, "<", des[n].name, " xmlns=\"",
			 NSPACE(des[n].nspace), "\"/>" EOL, NULL);
	ne_buffer_zappend(basic_search,
			  "</D:prop><D:descending/></D:order>" EOL);
    }

    ne_buffer_zappend(basic_search, "</D:orderby>" EOL);

    return NE_OK;
}


/* Search command parser
 *
 * This is the Search command parser implementation. 
 * The goal of this code is to translate
 * dasl search command into XML format.
 *
 * The user interface of search command is:
 *  search resource_URI display_fields condition orderby
 *
 *  This code will parse the three parts of the search command, 
 *  dispolay_fields,
 *   
 *  condition and orderby, and translate them into XML, which is a part of a 
 *  dasl request.
 * 
 * The BNF of display_fields, condition and orderby is as below:
 *
 *<display_fields> ::= identifier {,identifier}
 *<orderby clause> ::= identifier ["asc"|"desc"] {,identifier ["asc"|"desc"]}
 *
 *<search condition> ::= <boolean term> | <search condition> " or " <boolean term>
 *<boolean term> ::= <boolean factor> | <boolean term> " and " <boolean factor>
 *<boolean factor> ::= [not] <boolean primary>
 *<boolean primary> ::= <predicate>|"("<search condition>")"
 *<predicate> ::= <comparison predicate>|<like predicate>|<isdefined predicate>
 *<comparison predicate> ::=  <column_name> <comparaison_op> 
 *                            ( <number> | <quoted_string> | <wordstring>) 
 *<like predicate> ::= <column_name> like <quoted_string> -----Formally, 
 *                      [not] like match_string
 *<contains predicate> ::= contains (<quoted_string> | <wordstring>)
 *<column_name>
 *      ::= identifier
 *<comparaison_op>
 *      ::= "=" | "<" | ">" | "<>" | "!=" | "<=" | ">="
 *<quoted_string> ::= "'" {<any_character>} "'"
 *<wordstring> ::= <any char except for blank, tab and ')'>{<any_character except for blank, tab and ')'>} 
 *<match_string>
 *      ::= "'" { <any_character> | "_" | "%" } "'"
 *<identifier> ::= <letter> { <letter>|<digital>|<underline>}
 *<number> ::= [+|-]<digital>{<digital>}
 */

/* Read a word from the buffer. The word is fetched into 'word_fetched'. 
 * Every time a word is read, '*string_parsed' is changed to the pointer
 * pointing to the char just after 'word_fetched'.
 * 
 * Parameters:
 *     string_parsed:  -- *string_parsed points to the buffer. '*string_parsed'
 *                           will be changed every time a word is fetched.
 *     word_fetched:   -- The word that is fetched.
 * Returns:
 *     ENDBUF , if the end of buffer, '\0', has been reached.
 *     UNKNOWN, if the word is invalid.
 *     According word type, if the word is valid.
 * */
static int read_aword(char **string_parsed, char *word_fetched)
{
    int i = 0;
    int ifetch = 0;
    char *stringptr = *string_parsed;

    word_fetched[0] = '\0';

    /*Find the left bound of a word */
    while (isspace(stringptr[i]))
	i++;

    if (stringptr[i] == '\0')
	return ENDBUF;

    /*
     * Check whether the next word is a identifier. An identifier may
     * be a keyword or a column name.
     */
    if (isalpha(stringptr[i])) {
	do {
	    word_fetched[ifetch++] = stringptr[i++];
	} while (isalnum(stringptr[i]) || (stringptr[i] == '_'));

	*string_parsed = *string_parsed + i;
	word_fetched[ifetch] = '\0';
	return IDEN;		/*Identifier */
    }

    /* Check whether the next word is an integer */
    /* A problem here: + , or - !!!!! PK */
    if (stringptr[i] == '+' || stringptr[i] == '-' || isdigit(stringptr[i])) {
	do {
	    word_fetched[ifetch++] = stringptr[i++];
	} while (isdigit(stringptr[i]));

	*string_parsed = *string_parsed + i;
	word_fetched[ifetch] = '\0';
	return INTEGER;		/*Identifier */
    }

    /* Check whether the next word is "<=", ">=", or "<>" */
    if ((stringptr[i] == '>') && (stringptr[i + 1] == '=')) {
	strcpy(word_fetched, ">=");
	*string_parsed = *string_parsed + i + 2;
	return GE;		/*Indicates >= */
    }

    if ((stringptr[i] == '<') && (stringptr[i + 1] == '=')) {
	strcpy(word_fetched, "<=");
	*string_parsed = *string_parsed + i + 2;
	return LE;		/*Indicates <= */
    }

    if ((stringptr[i] == '<') && (stringptr[i + 1] == '>')) {
	strcpy(word_fetched, "<>");
	*string_parsed = *string_parsed + i + 2;
	return NEQ;		/*Indicates <> */
    }

    /*Check whether the next word is "'", ",", "(", ")", ">",
     * "<", "=", */
    switch (stringptr[i]) {
    case '\'':
	strcpy(word_fetched, "'");
	*string_parsed = *string_parsed + i + 1;
	return QUOT;
    case ',':
	strcpy(word_fetched, ",");
	*string_parsed = *string_parsed + i + 1;
	return COMMA;
    case '(':
	strcpy(word_fetched, "(");
	*string_parsed = *string_parsed + i + 1;
	return LPAR;
    case ')':
	strcpy(word_fetched, ")");
	*string_parsed = *string_parsed + i + 1;
	return RPAR;
    case '>':
	strcpy(word_fetched, ">");
	*string_parsed = *string_parsed + i + 1;
	return GT;
    case '<':
	strcpy(word_fetched, "<");
	*string_parsed = *string_parsed + i + 1;
	return LT;
    case '=':
	strcpy(word_fetched, "=");
	*string_parsed = *string_parsed + i + 1;
	return EQ;
    default:
	word_fetched[0] = stringptr[i];
	word_fetched[1] = '\0';
	*string_parsed = *string_parsed + i + 1;
	return UNKNOWN;
    }				/*End of switch */
}				/*End of read_aword */

/*Read the first word from the string buffer, string_parsed. If the
 * first is equal to the 'word_to_compare', return 1, else return 0.
 * The first word will be saved to 'the_first_word', but the pointer
 * to the buffer, string_parsed, will not be changed.
 * Returns:
 * 1: The first word is equal to the word expected.
 * 0: otherwise
 * */
static int first_word_equal(const char *string_parsed,
			    const char *word_to_compare)
{
    char *string_buffer = NULL;
    char *ptr_backup = NULL;
    char first_word[WORDLEN + 1] = "";

    string_buffer = ne_strdup(string_parsed);
    ptr_backup = string_buffer;

    read_aword(&string_buffer, first_word);

    NE_FREE(ptr_backup);

    if (strcasecmp(first_word, word_to_compare) == 0)
	return 1;		/*equal */
    else
	return 0;		/*Not equal */
}				/*End of first_word_compare */

/*Read the first word from the string buffer, string_parsed. If the
 * first is an interger, return 1, else return 0.
 * The first word will be saved to 'the_first_word', but the pointer
 * to the buffer, string_parsed, will not be changed.
 * Returns:
 * 1: The first word is equal to the word expected.
 * 0: otherwise
 * */
static int first_word_integer(const char *string_parsed)
{
    char *string_buffer = NULL;
    char *ptr_backup = NULL;
    char first_word[WORDLEN + 1] = "";
    int ret;

    string_buffer = ne_strdup(string_parsed);
    ptr_backup = string_buffer;

    if (read_aword(&string_buffer, first_word) == INTEGER)
	ret = 1;
    else
	ret = 0;

    NE_FREE(ptr_backup);

    return ret;
}				/*End of first_word_compare */

/* The function reads the first word in the string buffer, *string_parsed, and
 * changes *string_parsed. 
 * Returns:
 * NE_ERROR: If the word read is equal to 'str_expected'.
 * NE_OK: Otherwise
 * */
static int match_fetch(char **string_parsed, const char *str_expected)
{
    char first_word[WORDLEN + 1] = "";

    read_aword(string_parsed, first_word);

    if (strcmp(first_word, str_expected) == 0)
	return NE_OK;
    else
	return NE_ERROR;

}				/*End of match_fetch */

/*
 * Parse the search condition
 * <search condition> ::= <boolean term> | <search condition> or <boolean term>
 *
 * The parsing result is saved into result_buf.
 * '*string_parsed' is changed to the pointer pointing to the position after
 * <search condition>.
 * Returns:
 * NE_OK: success
 * NE_ERROR: syntax error
 * */
static int search_condition(char **string_parsed, ne_buffer * result_buf)
{
    char identifier[WORDLEN + 1] = "";
    /* Indicates whether there is any 'or' in the search condition */
    int added_or = 0;
    ne_buffer *term_result;

    ne_buffer_clear(result_buf);

    term_result = ne_buffer_create();

    if (boolean_term(string_parsed, term_result) == NE_ERROR) {
	ne_buffer_destroy(term_result);	/*Free the buffer */
	return NE_ERROR;	/*parsing error */
    }

    if (first_word_equal(*string_parsed, "or") == 1) {
	added_or = 1;
	ne_buffer_concat(result_buf, "<D:or>" EOL, term_result->data, NULL);
    }
    else
	ne_buffer_zappend(result_buf, term_result->data);

    /*For or <boolean term> or <boolean term> ... */
    while (first_word_equal(*string_parsed, "or") == 1) {
	read_aword(string_parsed, identifier);	/*Read 'or' */

	if (boolean_term(string_parsed, term_result) == NE_ERROR) {
	    ne_buffer_destroy(term_result);	/*Free the buffer */
	    return NE_ERROR;	/*Parsing error */
	}
	ne_buffer_zappend(result_buf, term_result->data);
    }				/*End of while */

    if (added_or == 1)
	ne_buffer_zappend(result_buf, "</D:or>" EOL);

    ne_buffer_destroy(term_result);	/*Free the buffer */

    return NE_OK;		/*success */
}				/*End of search_condition */

/*
 * Parse a boolean term.
 * <boolean term> ::= <boolean factor> | <boolean term> and <boolean factor>
 *
 * The parsing result is saved into result_buf.
 * '*string_parsed' is changed to the pointer pointing to the position after
 * <boolean term>.
 *
 * Returns:
 * NE_OK: success
 * NE_ERROR: syntax error
 */
static int boolean_term(char **string_parsed, ne_buffer * result_buf)
{
    char identifier[WORDLEN + 1] = "";
    /*Indicates whether there is any 'and' in the boolean term. */
    int added_and = 0;
    ne_buffer *factor_result;

    ne_buffer_clear(result_buf);
    factor_result = ne_buffer_create();

    if (boolean_factor(string_parsed, factor_result) == NE_ERROR) {
	ne_buffer_destroy(factor_result);
	return NE_ERROR;	/*parsing error */
    }

    if (first_word_equal(*string_parsed, "and") == 1) {
	added_and = 1;
	ne_buffer_concat(result_buf, "<D:and>" EOL, factor_result->data,
			 NULL);
    }
    else
	ne_buffer_zappend(result_buf, factor_result->data);

    while (first_word_equal(*string_parsed, "and") == 1) {
	read_aword(string_parsed, identifier);	/*Read 'and' */

	if (boolean_factor(string_parsed, factor_result) == NE_ERROR) {
	    ne_buffer_destroy(factor_result);
	    return NE_ERROR;	/*Parsing error */
	}
	ne_buffer_zappend(result_buf, factor_result->data);
    }				/*End of while */

    if (added_and == 1)
	ne_buffer_zappend(result_buf, "</D:and>" EOL);

    ne_buffer_destroy(factor_result);

    return NE_OK;		/*success */
}				/*End of boolean_term */

/*
 * Parse a boolean factor.
 * <boolean factor> ::= [not] <boolean primary>
 *
 * The parsing result is saved into result_buf.
 * '*string_parsed' is changed to the pointer pointing to the position after
 * <boolean factor>.
 *
 * Returns:
 * NE_OK: success
 * NE_ERROR: syntax error
 */
static int boolean_factor(char **string_parsed, ne_buffer * result_buf)
{
    char identifier[WORDLEN + 1] = "";
    int added_not = 0;		/*Indicates whether there is any 'not'
				   in the search condition */
    ne_buffer *boolean_primary_result;

    ne_buffer_clear(result_buf);
    boolean_primary_result = ne_buffer_create();

    if (first_word_equal(*string_parsed, "not") == 1) {
	read_aword(string_parsed, identifier);	/*Read "not" */
	ne_buffer_zappend(result_buf, "<D:not>" EOL);
	added_not = 1;
    }

    if (boolean_primary(string_parsed, boolean_primary_result) == NE_ERROR) {
	ne_buffer_destroy(boolean_primary_result);
	return NE_ERROR;	/*parsing error */
    }

    ne_buffer_zappend(result_buf, boolean_primary_result->data);

    if (added_not == 1)
	ne_buffer_zappend(result_buf, "</D:not>" EOL);

    ne_buffer_destroy(boolean_primary_result);

    return NE_OK;		/*success */
}				/*End of boolean_factor */

/*
 * Parse a boolean primary.
 * <boolean primary> ::= <predicate> | "("<search condition>")"
 *
 * The parsing result is saved into result_buf.
 * '*string_parsed' is changed to the pointer pointing to the position after
 * <boolean primary>.
 *
 * Returns:
 * NE_OK: success
 * NE_ERROR: syntax error
 */
static int boolean_primary(char **string_parsed, ne_buffer * result_buf)
{
    char identifier[WORDLEN + 1] = "";

    ne_buffer *sub_result;

    ne_buffer_clear(result_buf);
    sub_result = ne_buffer_create();

    if (first_word_equal(*string_parsed, "(") == 1) {
	/*It is the case of "("<search condition>")" */

	read_aword(string_parsed, identifier);	/*Read "(" */

	if (search_condition(string_parsed, sub_result) == NE_ERROR) {
	    ne_buffer_destroy(sub_result);
	    return NE_ERROR;	/*Parsing error */
	}

	/*Read and match ")" */
	if (match_fetch(string_parsed, ")") == NE_ERROR) {
	    ne_set_error(session,
			 "Syntax error: A ')' is expected in the search condition.");
	    ne_buffer_destroy(sub_result);
	    return NE_ERROR;	/*parsing error */
	}
	ne_buffer_zappend(result_buf, sub_result->data);
    }
    else {			/*It is the case of <predicate> */
	if (predicate(string_parsed, sub_result) == NE_ERROR) {
	    ne_buffer_destroy(sub_result);
	    return NE_ERROR;	/*parsing error */
	}
	ne_buffer_zappend(result_buf, sub_result->data);
    }

    ne_buffer_destroy(sub_result);

    return NE_OK;		/*success */
}				/*End of boolean_primary */

/*
 *  Translate comparison operator in the search condition to XML form.
 *  Returns:
 *    1: if the operator is valid.
 *    0: if the operator is not valid.
 */
static int operator_translate(const char *operator, char *XML_operator)
{
    int operator_valid = 1;

    XML_operator[0] = '\0';

    if (strcmp(operator, "=") == 0)
	strcpy(XML_operator, "eq");
    else if (strcmp(operator, ">=") == 0)
	strcpy(XML_operator, "gte");
    else if (strcmp(operator, "<=") == 0)
	strcpy(XML_operator, "lte");
    else if (strcmp(operator, ">") == 0)
	strcpy(XML_operator, "gt");
    else if (strcmp(operator, "<") == 0)
	strcpy(XML_operator, "lt");
    else if (strcmp(operator, "!=") == 0)
	strcpy(XML_operator, "not");
    else if (strcmp(operator, "<>") == 0)
	strcpy(XML_operator, "not");
    else if (strcmp(operator, "like") == 0)
	strcpy(XML_operator, "like");
    else
	operator_valid = 0;	/*Invalid operator */

    return operator_valid;
}				/*End of operator_translate */

/*
 * Parse a predicate.
 * <predicate> ::= <comparison predicate> | <like predicate> | <contains predicate>
 * <comparison predicate> ::=  <column_name> <comparaison_op> ( <number> | <quoted_string> | <wordstring>) 
 * <like predicate> ::= <column_name> like <match_string>
 * <contains predicate> ::= contains (<quoted_string> | <wordstring>)
 * <comparaison_op>
 *         ::= "=" | "<" | ">" | "<>" | "!=" | "<=" | ">="
 *   
 * The parsing result is saved into result_buf.
 * '*string_parsed' is changed to the pointer pointing to the position after
 * the predicate.
 *
 * Returns:
 * NE_OK: success
 * NE_ERROR: syntax error
 */
static int predicate(char **string_parsed, ne_buffer * result_buf)
{
    char column_name[WORDLEN + 1] = "";
    char operator[WORDLEN + 1] = "";
    char XML_operator[WORDLEN + 1] = "";
    ne_buffer *comparing_value;

    ne_buffer_clear(result_buf);

    if (first_word_equal(*string_parsed, "contains") == 1) {
	/* It is the case of <contains predicate> */
	ne_buffer *contains_result;
	contains_result = ne_buffer_create();
	if (contains_predicate(string_parsed, contains_result)
	    == NE_ERROR) {
	    ne_buffer_destroy(contains_result);
	    return NE_ERROR;	/*Parsing error */
	}
	ne_buffer_zappend(result_buf, contains_result->data);
	return NE_OK;
    }

    comparing_value = ne_buffer_create();

    /*Read the column name */
    if (read_aword(string_parsed, column_name) != IDEN) {
	ne_set_error(session,
		     "A column name is expected in the search condition.");
	ne_buffer_destroy(comparing_value);
	return NE_ERROR;	/*Parsing error */
    }

    /*Read the 'operator' */
    read_aword(string_parsed, operator);

    /*Translate the operator to XML form */
    if (operator_translate(operator, XML_operator) == 0) {
	ne_set_error(session,
		     "Syntax error: Invalid operator in the search condition.");
	ne_buffer_destroy(comparing_value);
	return NE_ERROR;
    }

    if (strcasecmp(operator, "like") == 0) {
	/*It is the case of like predicate and then parse the match string */
	if (first_word_equal(*string_parsed, "'")) {
	    /* For the case of a quoted string */
	    if (quoted_string(string_parsed, comparing_value) == NE_ERROR) {
		ne_buffer_destroy(comparing_value);
		return NE_ERROR;	/*Parsing error */
	    }
	}
	else {
	    /* For the case of word string. */
	    if (word_string(string_parsed, comparing_value) == NE_ERROR) {
		ne_set_error(session,
			     "Syntax error: A quoted string or a word string is expected in the search condition.");
		ne_buffer_destroy(comparing_value);
		return NE_ERROR;	/*Parsing error */
	    }
	}
    }
    else {
	/* It is the case of comparison predicate and then 
	 * parse the comparison value */
	if (comparison_value(string_parsed, comparing_value) == NE_ERROR) {
	    ne_buffer_destroy(comparing_value);
	    return NE_ERROR;	/*parsing error */
	}
    }

    ne_buffer_concat(result_buf, "<D:",
		     XML_operator,
		     ">" EOL
		     "<D:prop><D:",
		     column_name,
		     "/></D:prop>" EOL
		     "<D:literal>",
		     comparing_value->data,
		     "</D:literal>" EOL "</D:", XML_operator, ">" EOL, NULL);

    ne_buffer_destroy(comparing_value);

    return NE_OK;		/*success */
}				/*End of predicate */

/*
 * Parse a contains predicate.
 * <contains predicate> ::= contains <quoted_string> 
 *
 * The parsing result is saved into result_str.
 * '*string_parsed' is changed to the pointer pointing to the position after
 * the predicate.
 *
 * Returns:
 * 1: success
 * 0: syntax error
 */
int contains_predicate(char **string_parsed, ne_buffer * result_buf)
{
    ne_buffer *contain_string;

    ne_buffer_clear(result_buf);
    contain_string = ne_buffer_create();

    /*Read 'contains' */
    if (match_fetch(string_parsed, "contains") == NE_ERROR) {
	/*The case of <contains predicate> */
	ne_set_error(session,
		     "Syntax error: A 'contains' is expected in the search condition.");
	ne_buffer_destroy(contain_string);
	return NE_ERROR;
    }

    /*Now parse the containing string */
    if (first_word_equal(*string_parsed, "'") == 1) {
	/*For the case of <quoted string> */
	if (quoted_string(string_parsed, contain_string) == NE_ERROR) {
	    ne_buffer_destroy(contain_string);
	    return NE_ERROR;	/*Parsing error */
	}
    }
    else {
	/* For the case of <wordstring> */
	if (word_string(string_parsed, contain_string) == NE_ERROR) {
	    ne_set_error(session,
			 "Syntax error: A quoted string or a word string is expected in the search condition.");
	    ne_buffer_destroy(contain_string);
	    return NE_ERROR;	/*Parsing error */
	}
    }

    ne_buffer_concat(result_buf, "<D:contains> EOL",
		     contain_string->data, "</D:contains>" EOL, NULL);

    return NE_OK;		/*success */
}				/*End of contains_predicate */

/*
 * Parse a quoted string.
 * <quoted_string> ::= "'" {<any_character>} "'"
 *
 * The parsing result is saved into result_buf.
 * '*string_parsed' is changed to the pointer pointing to the position after
 * the predicate.
 *
 * Returns:
 * 1: success
 * 0: syntax error
 */
static int quoted_string(char **string_parsed, ne_buffer * result_buf)
{
    char previous_char;
    char current_char;
    char tmp_str[2] = "";
    int i = 0;

    ne_buffer_clear(result_buf);

    /*Read a quotation mark */
    if (match_fetch(string_parsed, "'") == NE_ERROR) {
	ne_set_error(session,
		     "Syntax error: A ' is expected in the search condition.");
	return NE_ERROR;	/*Parsing error */
    }

    /*To parse {"any_character"}. Considering '\'' case when parsing */
    current_char = previous_char = (*string_parsed)[0];
    while ( ((current_char != '\'') && (current_char != '\0')) ||
	    ((current_char == '\'') && (previous_char == '\\')) ) {
	tmp_str[0] = current_char;
	tmp_str[1] = '\0';
	ne_buffer_zappend(result_buf, tmp_str);
	previous_char = current_char;
	i++;
	current_char = (*string_parsed)[i];
    }

    if (current_char != '\'') {	/*There should be a ending ' */
	ne_set_error(session,
		     "An ending ' is expected in the search condition.");
	return NE_ERROR;	/*parsing error */
    }

    *string_parsed = *string_parsed + i + 1;	/*1 for ending ' */

    return NE_OK;		/*success */
}				/*End of auoted_string */

/*
 * Parse a word string.
 * <wordstring> ::= <any char except for blank, tab and ')'>{<any_character except for blank, tab and ')'>} 
 *
 * The parsing result is saved into result_buf.
 * '*string_parsed' is changed to the pointer pointing to the position after
 * the predicate.
 *
 * Returns:
 * 1: success
 * 0: syntax error
 */
static int word_string(char **string_parsed, ne_buffer * result_buf)
{
    char previous_char;
    char current_char;
    char tmp_str[2] = "";
    int i = 0;

    ne_buffer_clear(result_buf);

    /*Find the left bound of a word */
    while (isspace((*string_parsed)[i]))
	i++;

    if ((*string_parsed)[i] == '\0' || (*string_parsed)[i] == ')')	/* Empty string */
	return NE_ERROR;

    current_char = previous_char = (*string_parsed)[i];
    while ((current_char != ' ') &&
	   (current_char != '\t') &&
	   (current_char != ')') && (current_char != '\0')) {
	tmp_str[0] = current_char;
	tmp_str[1] = '\0';
	ne_buffer_zappend(result_buf, tmp_str);
	previous_char = current_char;
	i++;
	current_char = (*string_parsed)[i];
    }

    *string_parsed = *string_parsed + i;

    return NE_OK;		/*success */
}				/*End of auoted_string */

/*
 * Parse a comparison_value.
 *<comparison_value>
 *      ::=  <number> | <quoted_string> | <wordstring>
 *
 * The parsing result is saved into result_buf.
 * '*string_parsed' is changed to the pointer pointing to the position after
 * the predicate.
 *
 * Returns:
 * NE_OK: success
 * NE_ERROR: syntax error
 */
static int comparison_value(char **string_parsed, ne_buffer * result_buf)
{
    char identifier[WORDLEN + 1] = "";
    ne_buffer *comparing_value;

    ne_buffer_clear(result_buf);
    comparing_value = ne_buffer_create();

    if (first_word_equal(*string_parsed, "'") == 1) {
	/*It is the case of quoted string */
	if (quoted_string(string_parsed, comparing_value) == NE_ERROR) {
	    ne_buffer_destroy(comparing_value);
	    return NE_ERROR;	/*Parsing error */
	}
	ne_buffer_zappend(result_buf, comparing_value->data);
    }
    else {			/* It is the case of <number> or <wordstring> */
	/*An integer or a word string is expected */
	if (first_word_integer(*string_parsed) == 1) {
	    read_aword(string_parsed, identifier);
	    ne_buffer_zappend(result_buf, identifier);
	}
	else {			/* It is the case of a word string */
	    if (word_string(string_parsed, comparing_value) == NE_ERROR) {
		ne_set_error(session,
			     "Syntax error: An integer, quoted string or word string is expected in the search condition.");
		ne_buffer_destroy(comparing_value);
		return NE_ERROR;
	    }
	}
	ne_buffer_zappend(result_buf, comparing_value->data);
    }

    ne_buffer_destroy(comparing_value);

    return NE_OK;		/*success */
}				/*End of comparison_value */
