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

/* 0x00 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
/* 0x04 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
/* 0x08 */ BT_NONXML, BT_S, BT_LF, BT_NONXML,
/* 0x0C */ BT_NONXML, BT_CR, BT_NONXML, BT_NONXML,
/* 0x10 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
/* 0x14 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
/* 0x18 */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
/* 0x1C */ BT_NONXML, BT_NONXML, BT_NONXML, BT_NONXML,
/* 0x20 */ BT_S, BT_EXCL, BT_QUOT, BT_NUM,
/* 0x24 */ BT_OTHER, BT_PERCNT, BT_AMP, BT_APOS,
/* 0x28 */ BT_LPAR, BT_RPAR, BT_AST, BT_PLUS,
/* 0x2C */ BT_COMMA, BT_MINUS, BT_NAME, BT_SOL,
/* 0x30 */ BT_DIGIT, BT_DIGIT, BT_DIGIT, BT_DIGIT,
/* 0x34 */ BT_DIGIT, BT_DIGIT, BT_DIGIT, BT_DIGIT,
/* 0x38 */ BT_DIGIT, BT_DIGIT, BT_COLON, BT_SEMI,
/* 0x3C */ BT_LT, BT_EQUALS, BT_GT, BT_QUEST,
/* 0x40 */ BT_OTHER, BT_HEX, BT_HEX, BT_HEX,
/* 0x44 */ BT_HEX, BT_HEX, BT_HEX, BT_NMSTRT,
/* 0x48 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
/* 0x4C */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
/* 0x50 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
/* 0x54 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
/* 0x58 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_LSQB,
/* 0x5C */ BT_OTHER, BT_RSQB, BT_OTHER, BT_NMSTRT,
/* 0x60 */ BT_OTHER, BT_HEX, BT_HEX, BT_HEX,
/* 0x64 */ BT_HEX, BT_HEX, BT_HEX, BT_NMSTRT,
/* 0x68 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
/* 0x6C */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
/* 0x70 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
/* 0x74 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_NMSTRT,
/* 0x78 */ BT_NMSTRT, BT_NMSTRT, BT_NMSTRT, BT_OTHER,
/* 0x7C */ BT_VERBAR, BT_OTHER, BT_OTHER, BT_OTHER,
