/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/svg/SVGNumber.h"

#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGParserUtilities.h"

namespace blink {

SVGNumber::SVGNumber(float value) : m_value(value) {}

SVGNumber* SVGNumber::clone() const {
  return create(m_value);
}

String SVGNumber::valueAsString() const {
  return String::number(m_value);
}

template <typename CharType>
SVGParsingError SVGNumber::parse(const CharType*& ptr, const CharType* end) {
  float value = 0;
  const CharType* start = ptr;
  if (!parseNumber(ptr, end, value, AllowLeadingAndTrailingWhitespace))
    return SVGParsingError(SVGParseStatus::ExpectedNumber, ptr - start);
  if (ptr != end)
    return SVGParsingError(SVGParseStatus::TrailingGarbage, ptr - start);
  m_value = value;
  return SVGParseStatus::NoError;
}

SVGParsingError SVGNumber::setValueAsString(const String& string) {
  m_value = 0;

  if (string.isEmpty())
    return SVGParseStatus::NoError;

  if (string.is8Bit()) {
    const LChar* ptr = string.characters8();
    const LChar* end = ptr + string.length();
    return parse(ptr, end);
  }
  const UChar* ptr = string.characters16();
  const UChar* end = ptr + string.length();
  return parse(ptr, end);
}

void SVGNumber::add(SVGPropertyBase* other, SVGElement*) {
  setValue(m_value + toSVGNumber(other)->value());
}

void SVGNumber::calculateAnimatedValue(SVGAnimationElement* animationElement,
                                       float percentage,
                                       unsigned repeatCount,
                                       SVGPropertyBase* from,
                                       SVGPropertyBase* to,
                                       SVGPropertyBase* toAtEndOfDuration,
                                       SVGElement*) {
  ASSERT(animationElement);

  SVGNumber* fromNumber = toSVGNumber(from);
  SVGNumber* toNumber = toSVGNumber(to);
  SVGNumber* toAtEndOfDurationNumber = toSVGNumber(toAtEndOfDuration);

  animationElement->animateAdditiveNumber(
      percentage, repeatCount, fromNumber->value(), toNumber->value(),
      toAtEndOfDurationNumber->value(), m_value);
}

float SVGNumber::calculateDistance(SVGPropertyBase* other, SVGElement*) {
  return fabsf(m_value - toSVGNumber(other)->value());
}

SVGNumber* SVGNumberAcceptPercentage::clone() const {
  return create(m_value);
}

template <typename CharType>
static SVGParsingError parseNumberOrPercentage(const CharType*& ptr,
                                               const CharType* end,
                                               float& number) {
  const CharType* start = ptr;
  if (!parseNumber(ptr, end, number, AllowLeadingWhitespace))
    return SVGParsingError(SVGParseStatus::ExpectedNumberOrPercentage,
                           ptr - start);
  if (ptr < end && *ptr == '%') {
    number /= 100;
    ptr++;
  }
  if (skipOptionalSVGSpaces(ptr, end))
    return SVGParsingError(SVGParseStatus::TrailingGarbage, ptr - start);
  return SVGParseStatus::NoError;
}

SVGParsingError SVGNumberAcceptPercentage::setValueAsString(
    const String& string) {
  m_value = 0;

  if (string.isEmpty())
    return SVGParseStatus::ExpectedNumberOrPercentage;

  float number = 0;
  SVGParsingError error;
  if (string.is8Bit()) {
    const LChar* ptr = string.characters8();
    const LChar* end = ptr + string.length();
    error = parseNumberOrPercentage(ptr, end, number);
  } else {
    const UChar* ptr = string.characters16();
    const UChar* end = ptr + string.length();
    error = parseNumberOrPercentage(ptr, end, number);
  }
  if (error == SVGParseStatus::NoError)
    m_value = number;
  return error;
}

SVGNumberAcceptPercentage::SVGNumberAcceptPercentage(float value)
    : SVGNumber(value) {}

}  // namespace blink
