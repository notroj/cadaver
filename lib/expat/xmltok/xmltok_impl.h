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

enum {
  BT_NONXML,
  BT_MALFORM,
  BT_LT,
  BT_AMP,
  BT_RSQB,
  BT_LEAD2,
  BT_LEAD3,
  BT_LEAD4,
  BT_TRAIL,
  BT_CR,
  BT_LF,
  BT_GT,
  BT_QUOT,
  BT_APOS,
  BT_EQUALS,
  BT_QUEST,
  BT_EXCL,
  BT_SOL,
  BT_SEMI,
  BT_NUM,
  BT_LSQB,
  BT_S,
  BT_NMSTRT,
  BT_COLON,
  BT_HEX,
  BT_DIGIT,
  BT_NAME,
  BT_MINUS,
  BT_OTHER, /* known not to be a name or name start character */
  BT_NONASCII, /* might be a name or name start character */
  BT_PERCNT,
  BT_LPAR,
  BT_RPAR,
  BT_AST,
  BT_PLUS,
  BT_COMMA,
  BT_VERBAR
};

#include <stddef.h>
