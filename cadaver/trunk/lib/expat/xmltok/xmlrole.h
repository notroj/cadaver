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

#ifndef XmlRole_INCLUDED
#define XmlRole_INCLUDED 1

#include "xmltok.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  XML_ROLE_ERROR = -1,
  XML_ROLE_NONE = 0,
  XML_ROLE_XML_DECL,
  XML_ROLE_INSTANCE_START,
  XML_ROLE_DOCTYPE_NAME,
  XML_ROLE_DOCTYPE_SYSTEM_ID,
  XML_ROLE_DOCTYPE_PUBLIC_ID,
  XML_ROLE_DOCTYPE_CLOSE,
  XML_ROLE_GENERAL_ENTITY_NAME,
  XML_ROLE_PARAM_ENTITY_NAME,
  XML_ROLE_ENTITY_VALUE,
  XML_ROLE_ENTITY_SYSTEM_ID,
  XML_ROLE_ENTITY_PUBLIC_ID,
  XML_ROLE_ENTITY_NOTATION_NAME,
  XML_ROLE_NOTATION_NAME,
  XML_ROLE_NOTATION_SYSTEM_ID,
  XML_ROLE_NOTATION_NO_SYSTEM_ID,
  XML_ROLE_NOTATION_PUBLIC_ID,
  XML_ROLE_ATTRIBUTE_NAME,
  XML_ROLE_ATTRIBUTE_TYPE_CDATA,
  XML_ROLE_ATTRIBUTE_TYPE_ID,
  XML_ROLE_ATTRIBUTE_TYPE_IDREF,
  XML_ROLE_ATTRIBUTE_TYPE_IDREFS,
  XML_ROLE_ATTRIBUTE_TYPE_ENTITY,
  XML_ROLE_ATTRIBUTE_TYPE_ENTITIES,
  XML_ROLE_ATTRIBUTE_TYPE_NMTOKEN,
  XML_ROLE_ATTRIBUTE_TYPE_NMTOKENS,
  XML_ROLE_ATTRIBUTE_ENUM_VALUE,
  XML_ROLE_ATTRIBUTE_NOTATION_VALUE,
  XML_ROLE_ATTLIST_ELEMENT_NAME,
  XML_ROLE_IMPLIED_ATTRIBUTE_VALUE,
  XML_ROLE_REQUIRED_ATTRIBUTE_VALUE,
  XML_ROLE_DEFAULT_ATTRIBUTE_VALUE,
  XML_ROLE_FIXED_ATTRIBUTE_VALUE,
  XML_ROLE_ELEMENT_NAME,
  XML_ROLE_CONTENT_ANY,
  XML_ROLE_CONTENT_EMPTY,
  XML_ROLE_CONTENT_PCDATA,
  XML_ROLE_GROUP_OPEN,
  XML_ROLE_GROUP_CLOSE,
  XML_ROLE_GROUP_CLOSE_REP,
  XML_ROLE_GROUP_CLOSE_OPT,
  XML_ROLE_GROUP_CLOSE_PLUS,
  XML_ROLE_GROUP_CHOICE,
  XML_ROLE_GROUP_SEQUENCE,
  XML_ROLE_CONTENT_ELEMENT,
  XML_ROLE_CONTENT_ELEMENT_REP,
  XML_ROLE_CONTENT_ELEMENT_OPT,
  XML_ROLE_CONTENT_ELEMENT_PLUS,
#ifdef XML_DTD
  XML_ROLE_TEXT_DECL,
  XML_ROLE_IGNORE_SECT,
  XML_ROLE_INNER_PARAM_ENTITY_REF,
#endif /* XML_DTD */
  XML_ROLE_PARAM_ENTITY_REF
};

typedef struct prolog_state {
  int (*handler)(struct prolog_state *state,
	         int tok,
		 const char *ptr,
		 const char *end,
		 const ENCODING *enc);
  unsigned level;
#ifdef XML_DTD
  unsigned includeLevel;
  int documentEntity;
#endif /* XML_DTD */
} PROLOG_STATE;

void XMLTOKAPI XmlPrologStateInit(PROLOG_STATE *);
#ifdef XML_DTD
void XMLTOKAPI XmlPrologStateInitExternalEntity(PROLOG_STATE *);
#endif /* XML_DTD */

#define XmlTokenRole(state, tok, ptr, end, enc) \
 (((state)->handler)(state, tok, ptr, end, enc))

#ifdef __cplusplus
}
#endif

#endif /* not XmlRole_INCLUDED */
