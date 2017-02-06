/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "platform/text/TextBreakIterator.h"

#include "platform/text/Character.h"
#include "wtf/ASCIICType.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CharacterNames.h"

namespace blink {

unsigned numGraphemeClusters(const String& string)
{
    unsigned stringLength = string.length();

    if (!stringLength)
        return 0;

    // The only Latin-1 Extended Grapheme Cluster is CR LF
    if (string.is8Bit() && !string.contains('\r'))
        return stringLength;

    NonSharedCharacterBreakIterator it(string);
    if (!it)
        return stringLength;

    unsigned num = 0;
    while (it.next() != TextBreakDone)
        ++num;
    return num;
}


static inline bool isBreakableSpace(UChar ch)
{
    switch (ch) {
    case ' ':
    case '\n':
    case '\t':
        return true;
    default:
        return false;
    }
}

static const UChar asciiLineBreakTableFirstChar = '!';
static const UChar asciiLineBreakTableLastChar = 127;

// Pack 8 bits into one byte
#define B(a, b, c, d, e, f, g, h) \
    ((a) | ((b) << 1) | ((c) << 2) | ((d) << 3) | ((e) << 4) | ((f) << 5) | ((g) << 6) | ((h) << 7))

// Line breaking table row for each digit (0-9)
#define DI { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

// Line breaking table row for ascii letters (a-z A-Z)
#define AL { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

#define F 0xFF

// Line breaking table for printable ASCII characters. Line breaking opportunities in this table are as below:
// - before opening punctuations such as '(', '<', '[', '{' after certain characters (compatible with Firefox 3.6);
// - after '-' and '?' (backward-compatible, and compatible with Internet Explorer).
// Please refer to <https://bugs.webkit.org/show_bug.cgi?id=37698> for line breaking matrixes of different browsers
// and the ICU standard.
static const unsigned char asciiLineBreakTable[][(asciiLineBreakTableLastChar - asciiLineBreakTableFirstChar) / 8 + 1] = {
    //  !  "  #  $  %  &  '  (     )  *  +  ,  -  .  /  0  1-8   9  :  ;  <  =  >  ?  @     A-X      Y  Z  [  \  ]  ^  _  `     a-x      y  z  {  |  }  ~  DEL
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // !
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // "
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // #
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // $
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // %
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // &
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // '
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // (
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // )
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // *
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // +
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // ,
    { B(0, 1, 1, 1, 1, 1, 1, 1), B(0, 1, 1, 0, 1, 0, 0, 0), 0, B(0, 0, 0, 1, 1, 1, 0, 1), F, F, F, B(1, 1, 1, 1, 0, 1, 1, 1), F, F, F, B(1, 1, 1, 1, 0, 1, 1, 1) }, // - Note: breaking before '0'-'9' is handled hard-coded in shouldBreakAfter().
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // .
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // /
    DI,  DI,  DI,  DI,  DI,  DI,  DI,  DI,  DI,  DI, // 0-9
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // :
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // ;
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // <
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // =
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // >
    { B(0, 0, 1, 1, 1, 1, 0, 1), B(0, 1, 1, 0, 1, 0, 0, 1), F, B(1, 0, 0, 1, 1, 1, 0, 1), F, F, F, B(1, 1, 1, 1, 0, 1, 1, 1), F, F, F, B(1, 1, 1, 1, 0, 1, 1, 0) }, // ?
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // @
    AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL, // A-Z
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // [
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // '\'
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // ]
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // ^
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // _
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // `
    AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL,  AL, // a-z
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // {
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // |
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // }
    { B(0, 0, 0, 0, 0, 0, 0, 1), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 1, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 1, 0, 0, 0, 0, 0) }, // ~
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0), 0, 0, 0, B(0, 0, 0, 0, 0, 0, 0, 0) }, // DEL
};

// Line breaking table for CSS word-break: break-all. This table differs from
// asciiLineBreakTable in:
// - Indices are Line Breaking Classes defined in UAX#14 Unicode Line Breaking
//   Algorithm: http://unicode.org/reports/tr14/#DescriptionOfProperties
// - 1 indicates additional break opportunities. 0 indicates to fallback to
//   normal line break, not "prohibit break."
static const unsigned char breakAllLineBreakClassTable[][U_LB_COUNT / 8 + 1] = {
    // XX AI AL B2 BA BB BK CB    CL CM CR EX GL HY ID IN    IS LF NS NU OP PO PR QU    SA SG SP SY ZW NL WJ H2    H3 JL JT JV CP CJ HL RI
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // XX
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // AI
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // AL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // B2
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // BA
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // BB
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // BK
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // CB
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 0, 0, 1, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // CL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // CM
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // CR
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 0, 1, 1, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // EX
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // GL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 1, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // HY
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // ID
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // IN
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // IS
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // LF
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // NS
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // NU
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // OP
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 0, 1, 1, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // PO
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // PR
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // QU
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // SA
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // SG
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // SP
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // SY
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // ZW
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // NL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // WJ
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // H2
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // H3
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // JL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // JT
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // JV
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 0, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // CP
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // CJ
    { B(0, 1, 1, 0, 1, 0, 0, 0), B(0, 0, 0, 0, 0, 1, 0, 0), B(0, 0, 0, 1, 1, 0, 1, 0), B(1, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 1, 0) }, // HL
    { B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0), B(0, 0, 0, 0, 0, 0, 0, 0) }, // RI
};

#undef B
#undef F
#undef DI
#undef AL

static_assert(WTF_ARRAY_LENGTH(asciiLineBreakTable) == asciiLineBreakTableLastChar - asciiLineBreakTableFirstChar + 1, "asciiLineBreakTable should be consistent");
static_assert(WTF_ARRAY_LENGTH(breakAllLineBreakClassTable) == U_LB_COUNT, "breakAllLineBreakClassTable should be consistent");

static inline bool shouldBreakAfter(UChar lastCh, UChar ch, UChar nextCh)
{
    // Don't allow line breaking between '-' and a digit if the '-' may mean a minus sign in the context,
    // while allow breaking in 'ABCD-1234' and '1234-5678' which may be in long URLs.
    if (ch == '-' && isASCIIDigit(nextCh))
        return isASCIIAlphanumeric(lastCh);

    // If both ch and nextCh are ASCII characters, use a lookup table for enhanced speed and for compatibility
    // with other browsers (see comments for asciiLineBreakTable for details).
    if (ch >= asciiLineBreakTableFirstChar && ch <= asciiLineBreakTableLastChar
        && nextCh >= asciiLineBreakTableFirstChar && nextCh <= asciiLineBreakTableLastChar) {
        const unsigned char* tableRow = asciiLineBreakTable[ch - asciiLineBreakTableFirstChar];
        int nextChIndex = nextCh - asciiLineBreakTableFirstChar;
        return tableRow[nextChIndex / 8] & (1 << (nextChIndex % 8));
    }
    // Otherwise defer to the Unicode algorithm by returning false.
    return false;
}

static inline ULineBreak lineBreakPropertyValue(UChar lastCh, UChar ch)
{
    if (ch == '+') // IE tailors '+' to AL-like class when break-all is enabled.
        return U_LB_ALPHABETIC;
    UChar32 ch32 = U16_IS_LEAD(lastCh) && U16_IS_TRAIL(ch) ? U16_GET_SUPPLEMENTARY(lastCh, ch) : ch;
    return static_cast<ULineBreak>(u_getIntPropertyValue(ch32, UCHAR_LINE_BREAK));
}

static inline bool shouldBreakAfterBreakAll(ULineBreak lastLineBreak, ULineBreak lineBreak)
{
    if (lineBreak >= 0 && lineBreak < U_LB_COUNT && lastLineBreak >= 0 && lastLineBreak < U_LB_COUNT) {
        const unsigned char* tableRow = breakAllLineBreakClassTable[lastLineBreak];
        return tableRow[lineBreak / 8] & (1 << (lineBreak % 8));
    }
    return false;
}

inline bool needsLineBreakIterator(UChar ch)
{
    return ch > asciiLineBreakTableLastChar && ch != noBreakSpaceCharacter;
}

// Customization for ICU line breaking behavior. This allows us to reject ICU
// line break suggestions which would split an emoji sequence.
// FIXME crbug.com/593260: Remove this customization once ICU implements this
// natively.
static bool isBreakValid(const UChar* buf, size_t length, size_t breakPos)
{
    UChar32 codepoint;
    size_t prevOffset = breakPos;
    U16_PREV(buf, 0, prevOffset, codepoint);
    uint32_t nextCodepoint;
    size_t nextOffset = breakPos;
    U16_NEXT(buf, nextOffset, length, nextCodepoint);

    // Possible Emoji ZWJ sequence
    if (codepoint == zeroWidthJoinerCharacter) {
        if (nextCodepoint == 0x2764 // HEAVY BLACK HEART
            || nextCodepoint == 0x1F466 // BOY
            || nextCodepoint == 0x1F467 // GIRL
            || nextCodepoint == 0x1F468 // MAN
            || nextCodepoint == 0x1F469 // WOMAN
            || nextCodepoint == 0x1F48B // KISS MARK
            || nextCodepoint == 0x1F5E8) // LEFT SPEECH BUBBLE
        {
            return false;
        }
    }

    // Possible emoji modifier sequence
    // Proposed Rule LB30b from http://www.unicode.org/L2/L2016/16011r3-break-prop-emoji.pdf
    // EB x EM
    if (Character::isModifier(nextCodepoint)) {
        if (codepoint == variationSelector16Character && prevOffset > 0) {
            // Skip over emoji variation selector.
            U16_PREV(buf, 0, prevOffset, codepoint);
        }
        if (Character::isEmojiModifierBase(codepoint)) {
            return false;
        }
    }
    return true;
}

// Trivial implementation to match possible template paramters in
// nextBreakablePosition. There are no emoji sequences in 8bit strings, so we
// accept all break opportunities.
static bool isBreakValid(const LChar*, size_t, size_t)
{
    return true;
}

template<typename CharacterType, LineBreakType lineBreakType>
static inline int nextBreakablePosition(LazyLineBreakIterator& lazyBreakIterator, const CharacterType* str, unsigned length, int pos)
{
    int len = static_cast<int>(length);
    int nextBreak = -1;

    CharacterType lastLastCh = pos > 1 ? str[pos - 2] : static_cast<CharacterType>(lazyBreakIterator.secondToLastCharacter());
    CharacterType lastCh = pos > 0 ? str[pos - 1] : static_cast<CharacterType>(lazyBreakIterator.lastCharacter());
    ULineBreak lastLineBreak;
    if (lineBreakType == LineBreakType::BreakAll)
        lastLineBreak = lineBreakPropertyValue(lastLastCh, lastCh);
    unsigned priorContextLength = lazyBreakIterator.priorContextLength();
    for (int i = pos; i < len; i++) {
        CharacterType ch = str[i];

        if (isBreakableSpace(ch) || shouldBreakAfter(lastLastCh, lastCh, ch))
            return i;

        if (lineBreakType == LineBreakType::BreakAll && !U16_IS_LEAD(ch)) {
            ULineBreak lineBreak = lineBreakPropertyValue(lastCh, ch);
            if (shouldBreakAfterBreakAll(lastLineBreak, lineBreak))
                return i > pos && U16_IS_TRAIL(ch) ? i - 1 : i;
            if (lineBreak != U_LB_COMBINING_MARK)
                lastLineBreak = lineBreak;
        }

        if (needsLineBreakIterator(ch) || needsLineBreakIterator(lastCh)) {
            if (nextBreak < i) {
                // Don't break if positioned at start of primary context and there is no prior context.
                if (i || priorContextLength) {
                    TextBreakIterator* breakIterator = lazyBreakIterator.get(priorContextLength);
                    if (breakIterator) {
                        nextBreak = breakIterator->following(i - 1 + priorContextLength);
                        if (nextBreak >= 0) {
                            nextBreak -= priorContextLength;
                        }
                    }
                }
            }
            if (i == nextBreak && !isBreakableSpace(lastCh) && isBreakValid(str, length, i)) {
                return i;
            }
        }

        lastLastCh = lastCh;
        lastCh = ch;
    }

    return len;
}

static inline bool shouldKeepAfter(UChar lastCh, UChar ch, UChar nextCh)
{
    UChar preCh = U_MASK(u_charType(ch)) & U_GC_M_MASK ? lastCh : ch;
    return U_MASK(u_charType(preCh)) & (U_GC_L_MASK | U_GC_N_MASK)
        && !WTF::Unicode::hasLineBreakingPropertyComplexContext(preCh)
        && U_MASK(u_charType(nextCh)) & (U_GC_L_MASK | U_GC_N_MASK)
        && !WTF::Unicode::hasLineBreakingPropertyComplexContext(nextCh);
}

static inline int nextBreakablePositionKeepAllInternal(LazyLineBreakIterator& lazyBreakIterator, const UChar* str, unsigned length, int pos)
{
    int len = static_cast<int>(length);
    int nextBreak = -1;

    UChar lastLastCh = pos > 1 ? str[pos - 2] : static_cast<UChar>(lazyBreakIterator.secondToLastCharacter());
    UChar lastCh = pos > 0 ? str[pos - 1] : static_cast<UChar>(lazyBreakIterator.lastCharacter());
    unsigned priorContextLength = lazyBreakIterator.priorContextLength();
    for (int i = pos; i < len; i++) {
        UChar ch = str[i];

        if (isBreakableSpace(ch) || shouldBreakAfter(lastLastCh, lastCh, ch))
            return i;

        if (!shouldKeepAfter(lastLastCh, lastCh, ch) && (needsLineBreakIterator(ch) || needsLineBreakIterator(lastCh))) {
            if (nextBreak < i) {
                // Don't break if positioned at start of primary context and there is no prior context.
                if (i || priorContextLength) {
                    TextBreakIterator* breakIterator = lazyBreakIterator.get(priorContextLength);
                    if (breakIterator) {
                        nextBreak = breakIterator->following(i - 1 + priorContextLength);
                        if (nextBreak >= 0) {
                            nextBreak -= priorContextLength;
                        }
                    }
                }
            }
            if (i == nextBreak && !isBreakableSpace(lastCh) && isBreakValid(str, length, i))
                return i;
        }

        lastLastCh = lastCh;
        lastCh = ch;
    }

    return len;
}

template <LineBreakType lineBreakType>
static inline int nextBreakablePosition(LazyLineBreakIterator& lazyBreakIterator, const String& string, int pos)
{
    if (string.is8Bit())
        return nextBreakablePosition<LChar, lineBreakType>(lazyBreakIterator, string.characters8(), string.length(), pos);
    return nextBreakablePosition<UChar, lineBreakType>(lazyBreakIterator, string.characters16(), string.length(), pos);
}

int LazyLineBreakIterator::nextBreakablePositionIgnoringNBSP(int pos)
{
    return nextBreakablePosition<LineBreakType::Normal>(*this, m_string, pos);
}

int LazyLineBreakIterator::nextBreakablePositionBreakAll(int pos)
{
    return nextBreakablePosition<LineBreakType::BreakAll>(*this, m_string, pos);
}

int LazyLineBreakIterator::nextBreakablePositionKeepAll(int pos)
{
    if (m_string.is8Bit())
        return nextBreakablePosition<LChar, LineBreakType::Normal>(*this, m_string.characters8(), m_string.length(), pos);
    return nextBreakablePositionKeepAllInternal(*this, m_string.characters16(), m_string.length(), pos);
}

} // namespace blink
