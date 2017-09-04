// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSSyntaxDescriptor.h"

#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/parser/CSSParserIdioms.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/html/parser/HTMLParserIdioms.h"

namespace blink {

void consumeWhitespace(const String& string, size_t& offset) {
  while (isHTMLSpace(string[offset]))
    offset++;
}

bool consumeCharacterAndWhitespace(const String& string,
                                   char character,
                                   size_t& offset) {
  if (string[offset] != character)
    return false;
  offset++;
  consumeWhitespace(string, offset);
  return true;
}

CSSSyntaxType parseSyntaxType(String type) {
  // TODO(timloh): Are these supposed to be case sensitive?
  if (type == "length")
    return CSSSyntaxType::Length;
  if (type == "number")
    return CSSSyntaxType::Number;
  if (type == "percentage")
    return CSSSyntaxType::Percentage;
  if (type == "length-percentage")
    return CSSSyntaxType::LengthPercentage;
  if (type == "color")
    return CSSSyntaxType::Color;
  if (type == "image")
    return CSSSyntaxType::Image;
  if (type == "url")
    return CSSSyntaxType::Url;
  if (type == "integer")
    return CSSSyntaxType::Integer;
  if (type == "angle")
    return CSSSyntaxType::Angle;
  if (type == "time")
    return CSSSyntaxType::Time;
  if (type == "resolution")
    return CSSSyntaxType::Resolution;
  if (type == "transform-function")
    return CSSSyntaxType::TransformFunction;
  if (type == "custom-ident")
    return CSSSyntaxType::CustomIdent;
  // Not an Ident, just used to indicate failure
  return CSSSyntaxType::Ident;
}

bool consumeSyntaxType(const String& input,
                       size_t& offset,
                       CSSSyntaxType& type) {
  DCHECK_EQ(input[offset], '<');
  offset++;
  size_t typeStart = offset;
  while (offset < input.length() && input[offset] != '>')
    offset++;
  if (offset == input.length())
    return false;
  type = parseSyntaxType(input.substring(typeStart, offset - typeStart));
  if (type == CSSSyntaxType::Ident)
    return false;
  offset++;
  return true;
}

bool consumeSyntaxIdent(const String& input, size_t& offset, String& ident) {
  size_t identStart = offset;
  while (isNameCodePoint(input[offset]))
    offset++;
  if (offset == identStart)
    return false;
  ident = input.substring(identStart, offset - identStart);
  return !CSSPropertyParserHelpers::isCSSWideKeyword(ident);
}

CSSSyntaxDescriptor::CSSSyntaxDescriptor(String input) {
  size_t offset = 0;
  consumeWhitespace(input, offset);

  if (consumeCharacterAndWhitespace(input, '*', offset)) {
    if (offset != input.length())
      return;
    m_syntaxComponents.append(
        CSSSyntaxComponent(CSSSyntaxType::TokenStream, emptyString(), false));
    return;
  }

  do {
    CSSSyntaxType type;
    String ident;
    bool success;

    if (input[offset] == '<') {
      success = consumeSyntaxType(input, offset, type);
    } else {
      type = CSSSyntaxType::Ident;
      success = consumeSyntaxIdent(input, offset, ident);
    }

    if (!success) {
      m_syntaxComponents.clear();
      return;
    }

    bool repeatable = consumeCharacterAndWhitespace(input, '+', offset);
    consumeWhitespace(input, offset);
    m_syntaxComponents.append(CSSSyntaxComponent(type, ident, repeatable));

  } while (consumeCharacterAndWhitespace(input, '|', offset));

  if (offset != input.length())
    m_syntaxComponents.clear();
}

const CSSValue* consumeSingleType(const CSSSyntaxComponent& syntax,
                                  CSSParserTokenRange& range) {
  using namespace CSSPropertyParserHelpers;

  switch (syntax.m_type) {
    case CSSSyntaxType::Ident:
      if (range.peek().type() == IdentToken &&
          range.peek().value() == syntax.m_string) {
        range.consumeIncludingWhitespace();
        return CSSCustomIdentValue::create(AtomicString(syntax.m_string));
      }
      return nullptr;
    case CSSSyntaxType::Length:
      return consumeLength(range, HTMLStandardMode, ValueRange::ValueRangeAll);
    case CSSSyntaxType::Number:
      return consumeNumber(range, ValueRange::ValueRangeAll);
    case CSSSyntaxType::Percentage:
      return consumePercent(range, ValueRange::ValueRangeAll);
    case CSSSyntaxType::LengthPercentage:
      return consumeLengthOrPercent(range, HTMLStandardMode,
                                    ValueRange::ValueRangeAll);
    case CSSSyntaxType::Color:
      return consumeColor(range, HTMLStandardMode);
    case CSSSyntaxType::Image:
      // TODO(timloh): This probably needs a proper parser context for relative
      // URL resolution.
      return consumeImage(range, strictCSSParserContext());
    case CSSSyntaxType::Url:
      return consumeUrl(range);
    case CSSSyntaxType::Integer:
      return consumeInteger(range);
    case CSSSyntaxType::Angle:
      return consumeAngle(range);
    case CSSSyntaxType::Time:
      return consumeTime(range, ValueRange::ValueRangeAll);
    case CSSSyntaxType::Resolution:
      return nullptr;  // TODO(timloh): Implement this.
    case CSSSyntaxType::TransformFunction:
      return nullptr;  // TODO(timloh): Implement this.
    case CSSSyntaxType::CustomIdent:
      return consumeCustomIdent(range);
    default:
      NOTREACHED();
      return nullptr;
  }
}

const CSSValue* consumeSyntaxComponent(const CSSSyntaxComponent& syntax,
                                       CSSParserTokenRange range) {
  // CSS-wide keywords are already handled by the CSSPropertyParser
  if (syntax.m_repeatable) {
    CSSValueList* list = CSSValueList::createSpaceSeparated();
    while (!range.atEnd()) {
      const CSSValue* value = consumeSingleType(syntax, range);
      if (!value)
        return nullptr;
      list->append(*value);
    }
    return list;
  }
  const CSSValue* result = consumeSingleType(syntax, range);
  if (!range.atEnd())
    return nullptr;
  return result;
}

const CSSValue* CSSSyntaxDescriptor::parse(CSSParserTokenRange range,
                                           bool isAnimationTainted) const {
  if (isTokenStream()) {
    return CSSVariableParser::parseRegisteredPropertyValue(range, false,
                                                           isAnimationTainted);
  }
  range.consumeWhitespace();
  for (const CSSSyntaxComponent& component : m_syntaxComponents) {
    if (const CSSValue* result = consumeSyntaxComponent(component, range))
      return result;
  }
  return CSSVariableParser::parseRegisteredPropertyValue(range, true,
                                                         isAnimationTainted);
}

}  // namespace blink
