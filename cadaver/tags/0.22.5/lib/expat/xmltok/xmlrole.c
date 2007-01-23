/*
expat XML parser
Copyright (C) 1998 James Clark

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "xmldef.h"
#include "xmlrole.h"

/* Doesn't check:

 that ,| are not mixed in a model group
 content of literals

*/

#ifndef MIN_BYTES_PER_CHAR
#define MIN_BYTES_PER_CHAR(enc) ((enc)->minBytesPerChar)
#endif

#ifdef XML_DTD
#define setTopLevel(state) \
  ((state)->handler = ((state)->documentEntity \
                       ? internalSubset \
                       : externalSubset1))
#else /* not XML_DTD */
#define setTopLevel(state) ((state)->handler = internalSubset)
#endif /* not XML_DTD */

typedef int PROLOG_HANDLER(PROLOG_STATE *state,
			   int tok,
			   const char *ptr,
			   const char *end,
			   const ENCODING *enc);

static PROLOG_HANDLER
  prolog0, prolog1, prolog2,
  doctype0, doctype1, doctype2, doctype3, doctype4, doctype5,
  internalSubset,
  entity0, entity1, entity2, entity3, entity4, entity5, entity6,
  entity7, entity8, entity9,
  notation0, notation1, notation2, notation3, notation4,
  attlist0, attlist1, attlist2, attlist3, attlist4, attlist5, attlist6,
  attlist7, attlist8, attlist9,
  element0, element1, element2, element3, element4, element5, element6,
  element7,
#ifdef XML_DTD
  externalSubset0, externalSubset1,
  condSect0, condSect1, condSect2,
#endif /* XML_DTD */
  declClose,
  error;

static
int common(PROLOG_STATE *state, int tok);

static
int prolog0(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    state->handler = prolog1;
    return XML_ROLE_NONE;
  case XML_TOK_XML_DECL:
    state->handler = prolog1;
    return XML_ROLE_XML_DECL;
  case XML_TOK_PI:
    state->handler = prolog1;
    return XML_ROLE_NONE;
  case XML_TOK_COMMENT:
    state->handler = prolog1;
  case XML_TOK_BOM:
    return XML_ROLE_NONE;
  case XML_TOK_DECL_OPEN:
    if (!XmlNameMatchesAscii(enc,
			     ptr + 2 * MIN_BYTES_PER_CHAR(enc),
			     end,
			     "DOCTYPE"))
      break;
    state->handler = doctype0;
    return XML_ROLE_NONE;
  case XML_TOK_INSTANCE_START:
    state->handler = error;
    return XML_ROLE_INSTANCE_START;
  }
  return common(state, tok);
}

static
int prolog1(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_PI:
  case XML_TOK_COMMENT:
  case XML_TOK_BOM:
    return XML_ROLE_NONE;
  case XML_TOK_DECL_OPEN:
    if (!XmlNameMatchesAscii(enc,
			     ptr + 2 * MIN_BYTES_PER_CHAR(enc),
			     end,
			     "DOCTYPE"))
      break;
    state->handler = doctype0;
    return XML_ROLE_NONE;
  case XML_TOK_INSTANCE_START:
    state->handler = error;
    return XML_ROLE_INSTANCE_START;
  }
  return common(state, tok);
}

static
int prolog2(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_PI:
  case XML_TOK_COMMENT:
    return XML_ROLE_NONE;
  case XML_TOK_INSTANCE_START:
    state->handler = error;
    return XML_ROLE_INSTANCE_START;
  }
  return common(state, tok);
}

static
int doctype0(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
  case XML_TOK_PREFIXED_NAME:
    state->handler = doctype1;
    return XML_ROLE_DOCTYPE_NAME;
  }
  return common(state, tok);
}

static
int doctype1(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_OPEN_BRACKET:
    state->handler = internalSubset;
    return XML_ROLE_NONE;
  case XML_TOK_DECL_CLOSE:
    state->handler = prolog2;
    return XML_ROLE_DOCTYPE_CLOSE;
  case XML_TOK_NAME:
    if (XmlNameMatchesAscii(enc, ptr, end, "SYSTEM")) {
      state->handler = doctype3;
      return XML_ROLE_NONE;
    }
    if (XmlNameMatchesAscii(enc, ptr, end, "PUBLIC")) {
      state->handler = doctype2;
      return XML_ROLE_NONE;
    }
    break;
  }
  return common(state, tok);
}

static
int doctype2(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_LITERAL:
    state->handler = doctype3;
    return XML_ROLE_DOCTYPE_PUBLIC_ID;
  }
  return common(state, tok);
}

static
int doctype3(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_LITERAL:
    state->handler = doctype4;
    return XML_ROLE_DOCTYPE_SYSTEM_ID;
  }
  return common(state, tok);
}

static
int doctype4(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_OPEN_BRACKET:
    state->handler = internalSubset;
    return XML_ROLE_NONE;
  case XML_TOK_DECL_CLOSE:
    state->handler = prolog2;
    return XML_ROLE_DOCTYPE_CLOSE;
  }
  return common(state, tok);
}

static
int doctype5(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_DECL_CLOSE:
    state->handler = prolog2;
    return XML_ROLE_DOCTYPE_CLOSE;
  }
  return common(state, tok);
}

static
int internalSubset(PROLOG_STATE *state,
		   int tok,
		   const char *ptr,
		   const char *end,
		   const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_DECL_OPEN:
    if (XmlNameMatchesAscii(enc,
			    ptr + 2 * MIN_BYTES_PER_CHAR(enc),
			    end,
			    "ENTITY")) {
      state->handler = entity0;
      return XML_ROLE_NONE;
    }
    if (XmlNameMatchesAscii(enc,
			    ptr + 2 * MIN_BYTES_PER_CHAR(enc),
			    end,
			    "ATTLIST")) {
      state->handler = attlist0;
      return XML_ROLE_NONE;
    }
    if (XmlNameMatchesAscii(enc,
			    ptr + 2 * MIN_BYTES_PER_CHAR(enc),
			    end,
			    "ELEMENT")) {
      state->handler = element0;
      return XML_ROLE_NONE;
    }
    if (XmlNameMatchesAscii(enc,
			    ptr + 2 * MIN_BYTES_PER_CHAR(enc),
			    end,
			    "NOTATION")) {
      state->handler = notation0;
      return XML_ROLE_NONE;
    }
    break;
  case XML_TOK_PI:
  case XML_TOK_COMMENT:
    return XML_ROLE_NONE;
  case XML_TOK_PARAM_ENTITY_REF:
    return XML_ROLE_PARAM_ENTITY_REF;
  case XML_TOK_CLOSE_BRACKET:
    state->handler = doctype5;
    return XML_ROLE_NONE;
  }
  return common(state, tok);
}

#ifdef XML_DTD

static
int externalSubset0(PROLOG_STATE *state,
		    int tok,
		    const char *ptr,
		    const char *end,
		    const ENCODING *enc)
{
  state->handler = externalSubset1;
  if (tok == XML_TOK_XML_DECL)
    return XML_ROLE_TEXT_DECL;
  return externalSubset1(state, tok, ptr, end, enc);
}

static
int externalSubset1(PROLOG_STATE *state,
		    int tok,
		    const char *ptr,
		    const char *end,
		    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_COND_SECT_OPEN:
    state->handler = condSect0;
    return XML_ROLE_NONE;
  case XML_TOK_COND_SECT_CLOSE:
    if (state->includeLevel == 0)
      break;
    state->includeLevel -= 1;
    return XML_ROLE_NONE;
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_CLOSE_BRACKET:
    break;
  case XML_TOK_NONE:
    if (state->includeLevel)
      break;
    return XML_ROLE_NONE;
  default:
    return internalSubset(state, tok, ptr, end, enc);
  }
  return common(state, tok);
}

#endif /* XML_DTD */

static
int entity0(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_PERCENT:
    state->handler = entity1;
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    state->handler = entity2;
    return XML_ROLE_GENERAL_ENTITY_NAME;
  }
  return common(state, tok);
}

static
int entity1(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    state->handler = entity7;
    return XML_ROLE_PARAM_ENTITY_NAME;
  }
  return common(state, tok);
}

static
int entity2(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    if (XmlNameMatchesAscii(enc, ptr, end, "SYSTEM")) {
      state->handler = entity4;
      return XML_ROLE_NONE;
    }
    if (XmlNameMatchesAscii(enc, ptr, end, "PUBLIC")) {
      state->handler = entity3;
      return XML_ROLE_NONE;
    }
    break;
  case XML_TOK_LITERAL:
    state->handler = declClose;
    return XML_ROLE_ENTITY_VALUE;
  }
  return common(state, tok);
}

static
int entity3(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_LITERAL:
    state->handler = entity4;
    return XML_ROLE_ENTITY_PUBLIC_ID;
  }
  return common(state, tok);
}


static
int entity4(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_LITERAL:
    state->handler = entity5;
    return XML_ROLE_ENTITY_SYSTEM_ID;
  }
  return common(state, tok);
}

static
int entity5(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_DECL_CLOSE:
    setTopLevel(state);
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    if (XmlNameMatchesAscii(enc, ptr, end, "NDATA")) {
      state->handler = entity6;
      return XML_ROLE_NONE;
    }
    break;
  }
  return common(state, tok);
}

static
int entity6(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    state->handler = declClose;
    return XML_ROLE_ENTITY_NOTATION_NAME;
  }
  return common(state, tok);
}

static
int entity7(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    if (XmlNameMatchesAscii(enc, ptr, end, "SYSTEM")) {
      state->handler = entity9;
      return XML_ROLE_NONE;
    }
    if (XmlNameMatchesAscii(enc, ptr, end, "PUBLIC")) {
      state->handler = entity8;
      return XML_ROLE_NONE;
    }
    break;
  case XML_TOK_LITERAL:
    state->handler = declClose;
    return XML_ROLE_ENTITY_VALUE;
  }
  return common(state, tok);
}

static
int entity8(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_LITERAL:
    state->handler = entity9;
    return XML_ROLE_ENTITY_PUBLIC_ID;
  }
  return common(state, tok);
}

static
int entity9(PROLOG_STATE *state,
	    int tok,
	    const char *ptr,
	    const char *end,
	    const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_LITERAL:
    state->handler = declClose;
    return XML_ROLE_ENTITY_SYSTEM_ID;
  }
  return common(state, tok);
}

static
int notation0(PROLOG_STATE *state,
	      int tok,
	      const char *ptr,
	      const char *end,
	      const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    state->handler = notation1;
    return XML_ROLE_NOTATION_NAME;
  }
  return common(state, tok);
}

static
int notation1(PROLOG_STATE *state,
	      int tok,
	      const char *ptr,
	      const char *end,
	      const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    if (XmlNameMatchesAscii(enc, ptr, end, "SYSTEM")) {
      state->handler = notation3;
      return XML_ROLE_NONE;
    }
    if (XmlNameMatchesAscii(enc, ptr, end, "PUBLIC")) {
      state->handler = notation2;
      return XML_ROLE_NONE;
    }
    break;
  }
  return common(state, tok);
}

static
int notation2(PROLOG_STATE *state,
	      int tok,
	      const char *ptr,
	      const char *end,
	      const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_LITERAL:
    state->handler = notation4;
    return XML_ROLE_NOTATION_PUBLIC_ID;
  }
  return common(state, tok);
}

static
int notation3(PROLOG_STATE *state,
	      int tok,
	      const char *ptr,
	      const char *end,
	      const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_LITERAL:
    state->handler = declClose;
    return XML_ROLE_NOTATION_SYSTEM_ID;
  }
  return common(state, tok);
}

static
int notation4(PROLOG_STATE *state,
	      int tok,
	      const char *ptr,
	      const char *end,
	      const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_LITERAL:
    state->handler = declClose;
    return XML_ROLE_NOTATION_SYSTEM_ID;
  case XML_TOK_DECL_CLOSE:
    setTopLevel(state);
    return XML_ROLE_NOTATION_NO_SYSTEM_ID;
  }
  return common(state, tok);
}

static
int attlist0(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
  case XML_TOK_PREFIXED_NAME:
    state->handler = attlist1;
    return XML_ROLE_ATTLIST_ELEMENT_NAME;
  }
  return common(state, tok);
}

static
int attlist1(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_DECL_CLOSE:
    setTopLevel(state);
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
  case XML_TOK_PREFIXED_NAME:
    state->handler = attlist2;
    return XML_ROLE_ATTRIBUTE_NAME;
  }
  return common(state, tok);
}

static
int attlist2(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    {
      static const char *types[] = {
	"CDATA",
        "ID",
        "IDREF",
        "IDREFS",
        "ENTITY",
        "ENTITIES",
        "NMTOKEN",
        "NMTOKENS",
      };
      int i;
      for (i = 0; i < (int)(sizeof(types)/sizeof(types[0])); i++)
	if (XmlNameMatchesAscii(enc, ptr, end, types[i])) {
	  state->handler = attlist8;
	  return XML_ROLE_ATTRIBUTE_TYPE_CDATA + i;
	}
    }
    if (XmlNameMatchesAscii(enc, ptr, end, "NOTATION")) {
      state->handler = attlist5;
      return XML_ROLE_NONE;
    }
    break;
  case XML_TOK_OPEN_PAREN:
    state->handler = attlist3;
    return XML_ROLE_NONE;
  }
  return common(state, tok);
}

static
int attlist3(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NMTOKEN:
  case XML_TOK_NAME:
  case XML_TOK_PREFIXED_NAME:
    state->handler = attlist4;
    return XML_ROLE_ATTRIBUTE_ENUM_VALUE;
  }
  return common(state, tok);
}

static
int attlist4(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_CLOSE_PAREN:
    state->handler = attlist8;
    return XML_ROLE_NONE;
  case XML_TOK_OR:
    state->handler = attlist3;
    return XML_ROLE_NONE;
  }
  return common(state, tok);
}

static
int attlist5(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_OPEN_PAREN:
    state->handler = attlist6;
    return XML_ROLE_NONE;
  }
  return common(state, tok);
}


static
int attlist6(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    state->handler = attlist7;
    return XML_ROLE_ATTRIBUTE_NOTATION_VALUE;
  }
  return common(state, tok);
}

static
int attlist7(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_CLOSE_PAREN:
    state->handler = attlist8;
    return XML_ROLE_NONE;
  case XML_TOK_OR:
    state->handler = attlist6;
    return XML_ROLE_NONE;
  }
  return common(state, tok);
}

/* default value */
static
int attlist8(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_POUND_NAME:
    if (XmlNameMatchesAscii(enc,
			    ptr + MIN_BYTES_PER_CHAR(enc),
			    end,
			    "IMPLIED")) {
      state->handler = attlist1;
      return XML_ROLE_IMPLIED_ATTRIBUTE_VALUE;
    }
    if (XmlNameMatchesAscii(enc,
			    ptr + MIN_BYTES_PER_CHAR(enc),
			    end,
			    "REQUIRED")) {
      state->handler = attlist1;
      return XML_ROLE_REQUIRED_ATTRIBUTE_VALUE;
    }
    if (XmlNameMatchesAscii(enc,
			    ptr + MIN_BYTES_PER_CHAR(enc),
			    end,
			    "FIXED")) {
      state->handler = attlist9;
      return XML_ROLE_NONE;
    }
    break;
  case XML_TOK_LITERAL:
    state->handler = attlist1;
    return XML_ROLE_DEFAULT_ATTRIBUTE_VALUE;
  }
  return common(state, tok);
}

static
int attlist9(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_LITERAL:
    state->handler = attlist1;
    return XML_ROLE_FIXED_ATTRIBUTE_VALUE;
  }
  return common(state, tok);
}

static
int element0(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
  case XML_TOK_PREFIXED_NAME:
    state->handler = element1;
    return XML_ROLE_ELEMENT_NAME;
  }
  return common(state, tok);
}

static
int element1(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    if (XmlNameMatchesAscii(enc, ptr, end, "EMPTY")) {
      state->handler = declClose;
      return XML_ROLE_CONTENT_EMPTY;
    }
    if (XmlNameMatchesAscii(enc, ptr, end, "ANY")) {
      state->handler = declClose;
      return XML_ROLE_CONTENT_ANY;
    }
    break;
  case XML_TOK_OPEN_PAREN:
    state->handler = element2;
    state->level = 1;
    return XML_ROLE_GROUP_OPEN;
  }
  return common(state, tok);
}

static
int element2(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_POUND_NAME:
    if (XmlNameMatchesAscii(enc,
			    ptr + MIN_BYTES_PER_CHAR(enc),
			    end,
			    "PCDATA")) {
      state->handler = element3;
      return XML_ROLE_CONTENT_PCDATA;
    }
    break;
  case XML_TOK_OPEN_PAREN:
    state->level = 2;
    state->handler = element6;
    return XML_ROLE_GROUP_OPEN;
  case XML_TOK_NAME:
  case XML_TOK_PREFIXED_NAME:
    state->handler = element7;
    return XML_ROLE_CONTENT_ELEMENT;
  case XML_TOK_NAME_QUESTION:
    state->handler = element7;
    return XML_ROLE_CONTENT_ELEMENT_OPT;
  case XML_TOK_NAME_ASTERISK:
    state->handler = element7;
    return XML_ROLE_CONTENT_ELEMENT_REP;
  case XML_TOK_NAME_PLUS:
    state->handler = element7;
    return XML_ROLE_CONTENT_ELEMENT_PLUS;
  }
  return common(state, tok);
}

static
int element3(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_CLOSE_PAREN:
  case XML_TOK_CLOSE_PAREN_ASTERISK:
    state->handler = declClose;
    return XML_ROLE_GROUP_CLOSE_REP;
  case XML_TOK_OR:
    state->handler = element4;
    return XML_ROLE_NONE;
  }
  return common(state, tok);
}

static
int element4(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
  case XML_TOK_PREFIXED_NAME:
    state->handler = element5;
    return XML_ROLE_CONTENT_ELEMENT;
  }
  return common(state, tok);
}

static
int element5(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_CLOSE_PAREN_ASTERISK:
    state->handler = declClose;
    return XML_ROLE_GROUP_CLOSE_REP;
  case XML_TOK_OR:
    state->handler = element4;
    return XML_ROLE_NONE;
  }
  return common(state, tok);
}

static
int element6(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_OPEN_PAREN:
    state->level += 1;
    return XML_ROLE_GROUP_OPEN;
  case XML_TOK_NAME:
  case XML_TOK_PREFIXED_NAME:
    state->handler = element7;
    return XML_ROLE_CONTENT_ELEMENT;
  case XML_TOK_NAME_QUESTION:
    state->handler = element7;
    return XML_ROLE_CONTENT_ELEMENT_OPT;
  case XML_TOK_NAME_ASTERISK:
    state->handler = element7;
    return XML_ROLE_CONTENT_ELEMENT_REP;
  case XML_TOK_NAME_PLUS:
    state->handler = element7;
    return XML_ROLE_CONTENT_ELEMENT_PLUS;
  }
  return common(state, tok);
}

static
int element7(PROLOG_STATE *state,
	     int tok,
	     const char *ptr,
	     const char *end,
	     const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_CLOSE_PAREN:
    state->level -= 1;
    if (state->level == 0)
      state->handler = declClose;
    return XML_ROLE_GROUP_CLOSE;
  case XML_TOK_CLOSE_PAREN_ASTERISK:
    state->level -= 1;
    if (state->level == 0)
      state->handler = declClose;
    return XML_ROLE_GROUP_CLOSE_REP;
  case XML_TOK_CLOSE_PAREN_QUESTION:
    state->level -= 1;
    if (state->level == 0)
      state->handler = declClose;
    return XML_ROLE_GROUP_CLOSE_OPT;
  case XML_TOK_CLOSE_PAREN_PLUS:
    state->level -= 1;
    if (state->level == 0)
      state->handler = declClose;
    return XML_ROLE_GROUP_CLOSE_PLUS;
  case XML_TOK_COMMA:
    state->handler = element6;
    return XML_ROLE_GROUP_SEQUENCE;
  case XML_TOK_OR:
    state->handler = element6;
    return XML_ROLE_GROUP_CHOICE;
  }
  return common(state, tok);
}

#ifdef XML_DTD

static
int condSect0(PROLOG_STATE *state,
	      int tok,
	      const char *ptr,
	      const char *end,
	      const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_NAME:
    if (XmlNameMatchesAscii(enc, ptr, end, "INCLUDE")) {
      state->handler = condSect1;
      return XML_ROLE_NONE;
    }
    if (XmlNameMatchesAscii(enc, ptr, end, "IGNORE")) {
      state->handler = condSect2;
      return XML_ROLE_NONE;
    }
    break;
  }
  return common(state, tok);
}

static
int condSect1(PROLOG_STATE *state,
	      int tok,
	      const char *ptr,
	      const char *end,
	      const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_OPEN_BRACKET:
    state->handler = externalSubset1;
    state->includeLevel += 1;
    return XML_ROLE_NONE;
  }
  return common(state, tok);
}

static
int condSect2(PROLOG_STATE *state,
	      int tok,
	      const char *ptr,
	      const char *end,
	      const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_OPEN_BRACKET:
    state->handler = externalSubset1;
    return XML_ROLE_IGNORE_SECT;
  }
  return common(state, tok);
}

#endif /* XML_DTD */

static
int declClose(PROLOG_STATE *state,
	      int tok,
	      const char *ptr,
	      const char *end,
	      const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_PROLOG_S:
    return XML_ROLE_NONE;
  case XML_TOK_DECL_CLOSE:
    setTopLevel(state);
    return XML_ROLE_NONE;
  }
  return common(state, tok);
}

#if 0

static
int ignore(PROLOG_STATE *state,
	   int tok,
	   const char *ptr,
	   const char *end,
	   const ENCODING *enc)
{
  switch (tok) {
  case XML_TOK_DECL_CLOSE:
    state->handler = internalSubset;
    return 0;
  default:
    return XML_ROLE_NONE;
  }
  return common(state, tok);
}
#endif

static
int error(PROLOG_STATE *state,
	  int tok,
	  const char *ptr,
	  const char *end,
	  const ENCODING *enc)
{
  return XML_ROLE_NONE;
}

static
int common(PROLOG_STATE *state, int tok)
{
#ifdef XML_DTD
  if (!state->documentEntity && tok == XML_TOK_PARAM_ENTITY_REF)
    return XML_ROLE_INNER_PARAM_ENTITY_REF;
#endif
  state->handler = error;
  return XML_ROLE_ERROR;
}

void XmlPrologStateInit(PROLOG_STATE *state)
{
  state->handler = prolog0;
#ifdef XML_DTD
  state->documentEntity = 1;
  state->includeLevel = 0;
#endif /* XML_DTD */
}

#ifdef XML_DTD

void XmlPrologStateInitExternalEntity(PROLOG_STATE *state)
{
  state->handler = externalSubset0;
  state->documentEntity = 0;
  state->includeLevel = 0;
}

#endif /* XML_DTD */
