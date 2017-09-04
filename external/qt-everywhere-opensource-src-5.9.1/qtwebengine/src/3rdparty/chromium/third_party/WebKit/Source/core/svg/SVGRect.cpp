/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "core/svg/SVGRect.h"

#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

SVGRect::SVGRect() : m_isValid(true) {}

SVGRect::SVGRect(const FloatRect& rect) : m_isValid(true), m_value(rect) {}

SVGRect* SVGRect::clone() const {
  return SVGRect::create(m_value);
}

template <typename CharType>
SVGParsingError SVGRect::parse(const CharType*& ptr, const CharType* end) {
  const CharType* start = ptr;
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
  if (!parseNumber(ptr, end, x) || !parseNumber(ptr, end, y) ||
      !parseNumber(ptr, end, width) ||
      !parseNumber(ptr, end, height, DisallowWhitespace))
    return SVGParsingError(SVGParseStatus::ExpectedNumber, ptr - start);

  if (skipOptionalSVGSpaces(ptr, end)) {
    // Nothing should come after the last, fourth number.
    return SVGParsingError(SVGParseStatus::TrailingGarbage, ptr - start);
  }

  m_value = FloatRect(x, y, width, height);
  m_isValid = true;
  return SVGParseStatus::NoError;
}

SVGParsingError SVGRect::setValueAsString(const String& string) {
  setInvalid();

  if (string.isNull())
    return SVGParseStatus::NoError;

  if (string.isEmpty())
    return SVGParsingError(SVGParseStatus::ExpectedNumber, 0);

  if (string.is8Bit()) {
    const LChar* ptr = string.characters8();
    const LChar* end = ptr + string.length();
    return parse(ptr, end);
  }
  const UChar* ptr = string.characters16();
  const UChar* end = ptr + string.length();
  return parse(ptr, end);
}

String SVGRect::valueAsString() const {
  StringBuilder builder;
  builder.appendNumber(x());
  builder.append(' ');
  builder.appendNumber(y());
  builder.append(' ');
  builder.appendNumber(width());
  builder.append(' ');
  builder.appendNumber(height());
  return builder.toString();
}

void SVGRect::add(SVGPropertyBase* other, SVGElement*) {
  m_value += toSVGRect(other)->value();
}

void SVGRect::calculateAnimatedValue(SVGAnimationElement* animationElement,
                                     float percentage,
                                     unsigned repeatCount,
                                     SVGPropertyBase* fromValue,
                                     SVGPropertyBase* toValue,
                                     SVGPropertyBase* toAtEndOfDurationValue,
                                     SVGElement*) {
  ASSERT(animationElement);
  SVGRect* fromRect = animationElement->getAnimationMode() == ToAnimation
                          ? this
                          : toSVGRect(fromValue);
  SVGRect* toRect = toSVGRect(toValue);
  SVGRect* toAtEndOfDurationRect = toSVGRect(toAtEndOfDurationValue);

  float animatedX = x();
  float animatedY = y();
  float animatedWidth = width();
  float animatedHeight = height();
  animationElement->animateAdditiveNumber(
      percentage, repeatCount, fromRect->x(), toRect->x(),
      toAtEndOfDurationRect->x(), animatedX);
  animationElement->animateAdditiveNumber(
      percentage, repeatCount, fromRect->y(), toRect->y(),
      toAtEndOfDurationRect->y(), animatedY);
  animationElement->animateAdditiveNumber(
      percentage, repeatCount, fromRect->width(), toRect->width(),
      toAtEndOfDurationRect->width(), animatedWidth);
  animationElement->animateAdditiveNumber(
      percentage, repeatCount, fromRect->height(), toRect->height(),
      toAtEndOfDurationRect->height(), animatedHeight);

  m_value = FloatRect(animatedX, animatedY, animatedWidth, animatedHeight);
}

float SVGRect::calculateDistance(SVGPropertyBase* to,
                                 SVGElement* contextElement) {
  // FIXME: Distance calculation is not possible for SVGRect right now. We need
  // the distance for every single value.
  return -1;
}

void SVGRect::setInvalid() {
  m_value = FloatRect(0.0f, 0.0f, 0.0f, 0.0f);
  m_isValid = false;
}

}  // namespace blink
