/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
/* C++ code produced by gperf version 3.0.4 */
/* Command-line: gperf TokenLookup.gperf  */
/* Computed positions: -k'1,3,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 99 "TokenLookup.gperf"


QT_BEGIN_NAMESPACE

namespace QPatternist
{

#line 93 "TokenLookup.gperf"
struct TokenMap
{
    const char *name;
    const Tokenizer::TokenType token;
}


/* The strings below are in UTF-16 encoding. Subsequently, each ASCII
 * character is stored as the ASCII character, followed by a null byte.
 * Sorted alphabetically. */;
/* maximum key range = 228, duplicates = 0 */

class TokenLookup
{
private:
  static inline unsigned int hash (const char *str, unsigned int len);
public:
  static const struct TokenMap *value (const char *str, unsigned int len);
};

inline unsigned int
TokenLookup::hash (const char *str, unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230,  25, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230,   0,   2,   5,
       25,   0,  20,  20,  35,  85, 230, 230,  40, 110,
       25,  65,  80,   0,  60,   5,  10,   0,  55,   5,
       20,   0, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230, 230, 230, 230, 230,
      230, 230, 230, 230, 230, 230
    };
  int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[2]];
      /*FALLTHROUGH*/
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

const struct TokenMap *
TokenLookup::value (const char *str, unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 99,
      MIN_WORD_LENGTH = 2,
      MAX_WORD_LENGTH = 22,
      MIN_HASH_VALUE = 2,
      MAX_HASH_VALUE = 229
    };

  static const struct TokenMap wordlist[] =
    {
      {"",T_ERROR}, {"",T_ERROR},
#line 144 "TokenLookup.gperf"
      {"eq",                       T_EQ},
      {"",T_ERROR},
#line 122 "TokenLookup.gperf"
      {"by",                       T_BY},
#line 145 "TokenLookup.gperf"
      {"every",                    T_EVERY},
      {"",T_ERROR},
#line 115 "TokenLookup.gperf"
      {"as",                       T_AS},
      {"",T_ERROR},
#line 140 "TokenLookup.gperf"
      {"else",                     T_ELSE},
#line 209 "TokenLookup.gperf"
      {"where",                    T_WHERE},
#line 196 "TokenLookup.gperf"
      {"stable",                   T_STABLE},
#line 118 "TokenLookup.gperf"
      {"at",                       T_AT},
      {"",T_ERROR},
#line 123 "TokenLookup.gperf"
      {"case",                     T_CASE},
      {"",T_ERROR},
#line 121 "TokenLookup.gperf"
      {"boundary-space",           T_BOUNDARY_SPACE},
#line 139 "TokenLookup.gperf"
      {"element",                  T_ELEMENT},
#line 124 "TokenLookup.gperf"
      {"castable",                 T_CASTABLE},
#line 119 "TokenLookup.gperf"
      {"attribute",                T_ATTRIBUTE},
      {"",T_ERROR},
#line 146 "TokenLookup.gperf"
      {"except",                   T_EXCEPT},
#line 153 "TokenLookup.gperf"
      {"ge",                       T_GE},
      {"",T_ERROR},
#line 125 "TokenLookup.gperf"
      {"cast",                     T_CAST},
#line 202 "TokenLookup.gperf"
      {"treat",                    T_TREAT},
#line 210 "TokenLookup.gperf"
      {"xquery",                   T_XQUERY},
#line 173 "TokenLookup.gperf"
      {"ne",                       T_NE},
      {"",T_ERROR},
#line 190 "TokenLookup.gperf"
      {"satisfies",                T_SATISFIES},
      {"",T_ERROR}, {"",T_ERROR},
#line 155 "TokenLookup.gperf"
      {"gt",                       T_GT},
#line 143 "TokenLookup.gperf"
      {"encoding",                 T_ENCODING},
#line 116 "TokenLookup.gperf"
      {"ascending",                T_ASCENDING},
      {"",T_ERROR},
#line 117 "TokenLookup.gperf"
      {"assign",                   T_ASSIGN},
#line 131 "TokenLookup.gperf"
      {"declare",                  T_DECLARE},
#line 154 "TokenLookup.gperf"
      {"greatest",                 T_GREATEST},
#line 200 "TokenLookup.gperf"
      {"then",                     T_THEN},
      {"",T_ERROR},
#line 113 "TokenLookup.gperf"
      {"ancestor-or-self",         T_ANCESTOR_OR_SELF},
#line 167 "TokenLookup.gperf"
      {"le",                       T_LE},
#line 138 "TokenLookup.gperf"
      {"document-node",            T_DOCUMENT_NODE},
#line 199 "TokenLookup.gperf"
      {"text",                     T_TEXT},
      {"",T_ERROR},
#line 193 "TokenLookup.gperf"
      {"schema",                   T_SCHEMA},
      {"",T_ERROR},
#line 137 "TokenLookup.gperf"
      {"document",                 T_DOCUMENT},
      {"",T_ERROR},
#line 133 "TokenLookup.gperf"
      {"descendant",               T_DESCENDANT},
      {"",T_ERROR},
#line 169 "TokenLookup.gperf"
      {"lt",                       T_LT},
#line 114 "TokenLookup.gperf"
      {"and",                      T_AND},
#line 174 "TokenLookup.gperf"
      {"node",                     T_NODE},
#line 166 "TokenLookup.gperf"
      {"least",                    T_LEAST},
#line 191 "TokenLookup.gperf"
      {"schema-attribute",         T_SCHEMA_ATTRIBUTE},
      {"",T_ERROR},
#line 147 "TokenLookup.gperf"
      {"external",                 T_EXTERNAL},
      {"",T_ERROR},
#line 135 "TokenLookup.gperf"
      {"descending",               T_DESCENDING},
#line 176 "TokenLookup.gperf"
      {"no-preserve",              T_NO_PRESERVE},
#line 132 "TokenLookup.gperf"
      {"default",                  T_DEFAULT},
#line 168 "TokenLookup.gperf"
      {"let",                      T_LET},
#line 192 "TokenLookup.gperf"
      {"schema-element",           T_SCHEMA_ELEMENT},
      {"",T_ERROR}, {"",T_ERROR},
#line 129 "TokenLookup.gperf"
      {"construction",             T_CONSTRUCTION},
#line 134 "TokenLookup.gperf"
      {"descendant-or-self",       T_DESCENDANT_OR_SELF},
#line 194 "TokenLookup.gperf"
      {"self",                     T_SELF},
#line 175 "TokenLookup.gperf"
      {"no-inherit",               T_NO_INHERIT},
      {"",T_ERROR},
#line 150 "TokenLookup.gperf"
      {"follows",                  T_FOLLOWS},
#line 112 "TokenLookup.gperf"
      {"ancestor",                 T_ANCESTOR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
#line 201 "TokenLookup.gperf"
      {"to",                       T_TO},
#line 152 "TokenLookup.gperf"
      {"function",                 T_FUNCTION},
#line 127 "TokenLookup.gperf"
      {"collation",                T_COLLATION},
      {"",T_ERROR},
#line 197 "TokenLookup.gperf"
      {"strict",                   T_STRICT},
      {"",T_ERROR},
#line 165 "TokenLookup.gperf"
      {"lax",                      T_LAX},
      {"",T_ERROR},
#line 141 "TokenLookup.gperf"
      {"empty",                    T_EMPTY},
      {"",T_ERROR},
#line 177 "TokenLookup.gperf"
      {"of",                       T_OF},
#line 187 "TokenLookup.gperf"
      {"preserve",                 T_PRESERVE},
#line 148 "TokenLookup.gperf"
      {"following",                T_FOLLOWING},
      {"",T_ERROR}, {"",T_ERROR},
#line 163 "TokenLookup.gperf"
      {"is",                       T_IS},
#line 184 "TokenLookup.gperf"
      {"precedes",                 T_PRECEDES},
#line 142 "TokenLookup.gperf"
      {"empty-sequence",           T_EMPTY_SEQUENCE},
      {"",T_ERROR}, {"",T_ERROR},
#line 149 "TokenLookup.gperf"
      {"following-sibling",        T_FOLLOWING_SIBLING},
#line 161 "TokenLookup.gperf"
      {"instance",                 T_INSTANCE},
#line 205 "TokenLookup.gperf"
      {"unordered",                T_UNORDERED},
#line 120 "TokenLookup.gperf"
      {"base-uri",                 T_BASEURI},
#line 189 "TokenLookup.gperf"
      {"return",                   T_RETURN},
      {"",T_ERROR},
#line 206 "TokenLookup.gperf"
      {"validate",                 T_VALIDATE},
      {"",T_ERROR},
#line 130 "TokenLookup.gperf"
      {"copy-namespaces",          T_COPY_NAMESPACES},
#line 178 "TokenLookup.gperf"
      {"option",                   T_OPTION},
#line 157 "TokenLookup.gperf"
      {"if",                       T_IF},
      {"",T_ERROR},
#line 185 "TokenLookup.gperf"
      {"preceding",                T_PRECEDING},
      {"",T_ERROR}, {"",T_ERROR},
#line 160 "TokenLookup.gperf"
      {"in",                       T_IN},
      {"",T_ERROR},
#line 162 "TokenLookup.gperf"
      {"intersect",                T_INTERSECT},
#line 204 "TokenLookup.gperf"
      {"union",                    T_UNION},
      {"",T_ERROR},
#line 186 "TokenLookup.gperf"
      {"preceding-sibling",        T_PRECEDING_SIBLING},
#line 180 "TokenLookup.gperf"
      {"ordering",                 T_ORDERING},
#line 195 "TokenLookup.gperf"
      {"some",                     T_SOME},
#line 126 "TokenLookup.gperf"
      {"child",                    T_CHILD},
      {"",T_ERROR},
#line 179 "TokenLookup.gperf"
      {"ordered",                  T_ORDERED},
#line 207 "TokenLookup.gperf"
      {"variable",                 T_VARIABLE},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
#line 182 "TokenLookup.gperf"
      {"or",                       T_OR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
#line 128 "TokenLookup.gperf"
      {"comment",                  T_COMMENT},
      {"",T_ERROR}, {"",T_ERROR},
#line 203 "TokenLookup.gperf"
      {"typeswitch",               T_TYPESWITCH},
      {"",T_ERROR},
#line 159 "TokenLookup.gperf"
      {"inherit",                  T_INHERIT},
#line 136 "TokenLookup.gperf"
      {"div",                      T_DIV},
      {"",T_ERROR}, {"",T_ERROR},
#line 171 "TokenLookup.gperf"
      {"module",                   T_MODULE},
      {"",T_ERROR},
#line 151 "TokenLookup.gperf"
      {"for",                      T_FOR},
#line 172 "TokenLookup.gperf"
      {"namespace",                T_NAMESPACE},
      {"",T_ERROR}, {"",T_ERROR},
#line 208 "TokenLookup.gperf"
      {"version",                  T_VERSION},
      {"",T_ERROR}, {"",T_ERROR},
#line 198 "TokenLookup.gperf"
      {"strip",                    T_STRIP},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
#line 181 "TokenLookup.gperf"
      {"order",                    T_ORDER},
#line 183 "TokenLookup.gperf"
      {"parent",                   T_PARENT},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR},
#line 170 "TokenLookup.gperf"
      {"mod",                      T_MOD},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR},
#line 158 "TokenLookup.gperf"
      {"import",                   T_IMPORT},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR},
#line 188 "TokenLookup.gperf"
      {"processing-instruction",   T_PROCESSING_INSTRUCTION},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR},
#line 164 "TokenLookup.gperf"
      {"item",                     T_ITEM},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR}, {"",T_ERROR},
      {"",T_ERROR},
#line 156 "TokenLookup.gperf"
      {"idiv",                     T_IDIV}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
#line 211 "TokenLookup.gperf"


} /* Close the QPatternist namespace. */

QT_END_NAMESPACE
