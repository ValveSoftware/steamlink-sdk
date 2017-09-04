// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserToken.h"

#include "core/css/CSSMarkup.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringBuilder.h"
#include <limits.h>

namespace blink {

CSSParserToken::CSSParserToken(CSSParserTokenType type, BlockType blockType)
    : m_type(type), m_blockType(blockType) {}

// Just a helper used for Delimiter tokens.
CSSParserToken::CSSParserToken(CSSParserTokenType type, UChar c)
    : m_type(type), m_blockType(NotBlock), m_delimiter(c) {
  ASSERT(m_type == DelimiterToken);
}

CSSParserToken::CSSParserToken(CSSParserTokenType type,
                               StringView value,
                               BlockType blockType)
    : m_type(type), m_blockType(blockType) {
  initValueFromStringView(value);
  m_id = -1;
}

CSSParserToken::CSSParserToken(CSSParserTokenType type,
                               double numericValue,
                               NumericValueType numericValueType,
                               NumericSign sign)
    : m_type(type),
      m_blockType(NotBlock),
      m_numericValueType(numericValueType),
      m_numericSign(sign),
      m_unit(static_cast<unsigned>(CSSPrimitiveValue::UnitType::Number)) {
  ASSERT(type == NumberToken);
  m_numericValue =
      clampTo<double>(numericValue, -std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max());
}

CSSParserToken::CSSParserToken(CSSParserTokenType type,
                               UChar32 start,
                               UChar32 end)
    : m_type(UnicodeRangeToken), m_blockType(NotBlock) {
  DCHECK_EQ(type, UnicodeRangeToken);
  m_unicodeRange.start = start;
  m_unicodeRange.end = end;
}

CSSParserToken::CSSParserToken(HashTokenType type, StringView value)
    : m_type(HashToken), m_blockType(NotBlock), m_hashTokenType(type) {
  initValueFromStringView(value);
}

void CSSParserToken::convertToDimensionWithUnit(StringView unit) {
  ASSERT(m_type == NumberToken);
  m_type = DimensionToken;
  initValueFromStringView(unit);
  m_unit = static_cast<unsigned>(CSSPrimitiveValue::stringToUnitType(unit));
}

void CSSParserToken::convertToPercentage() {
  ASSERT(m_type == NumberToken);
  m_type = PercentageToken;
  m_unit = static_cast<unsigned>(CSSPrimitiveValue::UnitType::Percentage);
}

UChar CSSParserToken::delimiter() const {
  ASSERT(m_type == DelimiterToken);
  return m_delimiter;
}

NumericSign CSSParserToken::numericSign() const {
  // This is valid for DimensionToken and PercentageToken, but only used
  // in <an+b> parsing on NumberTokens.
  ASSERT(m_type == NumberToken);
  return static_cast<NumericSign>(m_numericSign);
}

NumericValueType CSSParserToken::numericValueType() const {
  ASSERT(m_type == NumberToken || m_type == PercentageToken ||
         m_type == DimensionToken);
  return static_cast<NumericValueType>(m_numericValueType);
}

double CSSParserToken::numericValue() const {
  ASSERT(m_type == NumberToken || m_type == PercentageToken ||
         m_type == DimensionToken);
  return m_numericValue;
}

CSSPropertyID CSSParserToken::parseAsUnresolvedCSSPropertyID() const {
  ASSERT(m_type == IdentToken);
  return unresolvedCSSPropertyID(value());
}

CSSValueID CSSParserToken::id() const {
  if (m_type != IdentToken)
    return CSSValueInvalid;
  if (m_id < 0)
    m_id = cssValueKeywordID(value());
  return static_cast<CSSValueID>(m_id);
}

CSSValueID CSSParserToken::functionId() const {
  if (m_type != FunctionToken)
    return CSSValueInvalid;
  if (m_id < 0)
    m_id = cssValueKeywordID(value());
  return static_cast<CSSValueID>(m_id);
}

bool CSSParserToken::hasStringBacking() const {
  CSSParserTokenType tokenType = type();
  return tokenType == IdentToken || tokenType == FunctionToken ||
         tokenType == AtKeywordToken || tokenType == HashToken ||
         tokenType == UrlToken || tokenType == DimensionToken ||
         tokenType == StringToken;
}

CSSParserToken CSSParserToken::copyWithUpdatedString(
    const StringView& string) const {
  CSSParserToken copy(*this);
  copy.initValueFromStringView(string);
  return copy;
}

bool CSSParserToken::valueDataCharRawEqual(const CSSParserToken& other) const {
  if (m_valueLength != other.m_valueLength)
    return false;

  if (m_valueDataCharRaw == other.m_valueDataCharRaw &&
      m_valueIs8Bit == other.m_valueIs8Bit)
    return true;

  if (m_valueIs8Bit) {
    return other.m_valueIs8Bit
               ? equal(static_cast<const LChar*>(m_valueDataCharRaw),
                       static_cast<const LChar*>(other.m_valueDataCharRaw),
                       m_valueLength)
               : equal(static_cast<const LChar*>(m_valueDataCharRaw),
                       static_cast<const UChar*>(other.m_valueDataCharRaw),
                       m_valueLength);
  } else {
    return other.m_valueIs8Bit
               ? equal(static_cast<const UChar*>(m_valueDataCharRaw),
                       static_cast<const LChar*>(other.m_valueDataCharRaw),
                       m_valueLength)
               : equal(static_cast<const UChar*>(m_valueDataCharRaw),
                       static_cast<const UChar*>(other.m_valueDataCharRaw),
                       m_valueLength);
  }
}

bool CSSParserToken::operator==(const CSSParserToken& other) const {
  if (m_type != other.m_type)
    return false;
  switch (m_type) {
    case DelimiterToken:
      return delimiter() == other.delimiter();
    case HashToken:
      if (m_hashTokenType != other.m_hashTokenType)
        return false;
    // fallthrough
    case IdentToken:
    case FunctionToken:
    case StringToken:
    case UrlToken:
      return valueDataCharRawEqual(other);
    case DimensionToken:
      if (!valueDataCharRawEqual(other))
        return false;
    // fallthrough
    case NumberToken:
    case PercentageToken:
      return m_numericSign == other.m_numericSign &&
             m_numericValue == other.m_numericValue &&
             m_numericValueType == other.m_numericValueType;
    case UnicodeRangeToken:
      return m_unicodeRange.start == other.m_unicodeRange.start &&
             m_unicodeRange.end == other.m_unicodeRange.end;
    default:
      return true;
  }
}

void CSSParserToken::serialize(StringBuilder& builder) const {
  // This is currently only used for @supports CSSOM. To keep our implementation
  // simple we handle some of the edge cases incorrectly (see comments below).
  switch (type()) {
    case IdentToken:
      serializeIdentifier(value().toString(), builder);
      break;
    case FunctionToken:
      serializeIdentifier(value().toString(), builder);
      return builder.append('(');
    case AtKeywordToken:
      builder.append('@');
      serializeIdentifier(value().toString(), builder);
      break;
    case HashToken:
      builder.append('#');
      serializeIdentifier(value().toString(), builder,
                          (getHashTokenType() == HashTokenUnrestricted));
      break;
    case UrlToken:
      builder.append("url(");
      serializeIdentifier(value().toString(), builder);
      return builder.append(')');
    case DelimiterToken:
      if (delimiter() == '\\')
        return builder.append("\\\n");
      return builder.append(delimiter());
    case NumberToken:
      // These won't properly preserve the NumericValueType flag
      return builder.appendNumber(numericValue());
    case PercentageToken:
      builder.appendNumber(numericValue());
      return builder.append('%');
    case DimensionToken:
      // This will incorrectly serialize e.g. 4e3e2 as 4000e2
      builder.appendNumber(numericValue());
      serializeIdentifier(value().toString(), builder);
      break;
    case UnicodeRangeToken:
      return builder.append(
          String::format("U+%X-%X", unicodeRangeStart(), unicodeRangeEnd()));
    case StringToken:
      return serializeString(value().toString(), builder);

    case IncludeMatchToken:
      return builder.append("~=");
    case DashMatchToken:
      return builder.append("|=");
    case PrefixMatchToken:
      return builder.append("^=");
    case SuffixMatchToken:
      return builder.append("$=");
    case SubstringMatchToken:
      return builder.append("*=");
    case ColumnToken:
      return builder.append("||");
    case CDOToken:
      return builder.append("<!--");
    case CDCToken:
      return builder.append("-->");
    case BadStringToken:
      return builder.append("'\n");
    case BadUrlToken:
      return builder.append("url(()");
    case WhitespaceToken:
      return builder.append(' ');
    case ColonToken:
      return builder.append(':');
    case SemicolonToken:
      return builder.append(';');
    case CommaToken:
      return builder.append(',');
    case LeftParenthesisToken:
      return builder.append('(');
    case RightParenthesisToken:
      return builder.append(')');
    case LeftBracketToken:
      return builder.append('[');
    case RightBracketToken:
      return builder.append(']');
    case LeftBraceToken:
      return builder.append('{');
    case RightBraceToken:
      return builder.append('}');

    case EOFToken:
    case CommentToken:
      ASSERT_NOT_REACHED();
      return;
  }
}

}  // namespace blink
