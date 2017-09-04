/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGPointList.h"

#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "platform/geometry/FloatPoint.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

SVGPointList::SVGPointList() {}

SVGPointList::~SVGPointList() {}

String SVGPointList::valueAsString() const {
  StringBuilder builder;

  ConstIterator it = begin();
  ConstIterator itEnd = end();
  if (it != itEnd) {
    builder.append(it->valueAsString());
    ++it;

    for (; it != itEnd; ++it) {
      builder.append(' ');
      builder.append(it->valueAsString());
    }
  }

  return builder.toString();
}

template <typename CharType>
SVGParsingError SVGPointList::parse(const CharType*& ptr, const CharType* end) {
  if (!skipOptionalSVGSpaces(ptr, end))
    return SVGParseStatus::NoError;

  const CharType* listStart = ptr;
  for (;;) {
    float x = 0;
    float y = 0;
    if (!parseNumber(ptr, end, x) ||
        !parseNumber(ptr, end, y, DisallowWhitespace))
      return SVGParsingError(SVGParseStatus::ExpectedNumber, ptr - listStart);

    append(SVGPoint::create(FloatPoint(x, y)));

    if (!skipOptionalSVGSpaces(ptr, end))
      break;

    if (*ptr == ',') {
      ++ptr;
      skipOptionalSVGSpaces(ptr, end);

      // ',' requires the list to be continued
      continue;
    }
  }
  return SVGParseStatus::NoError;
}

SVGParsingError SVGPointList::setValueAsString(const String& value) {
  clear();

  if (value.isEmpty())
    return SVGParseStatus::NoError;

  if (value.is8Bit()) {
    const LChar* ptr = value.characters8();
    const LChar* end = ptr + value.length();
    return parse(ptr, end);
  }
  const UChar* ptr = value.characters16();
  const UChar* end = ptr + value.length();
  return parse(ptr, end);
}

void SVGPointList::add(SVGPropertyBase* other, SVGElement* contextElement) {
  SVGPointList* otherList = toSVGPointList(other);

  if (length() != otherList->length())
    return;

  for (size_t i = 0; i < length(); ++i)
    at(i)->setValue(at(i)->value() + otherList->at(i)->value());
}

void SVGPointList::calculateAnimatedValue(
    SVGAnimationElement* animationElement,
    float percentage,
    unsigned repeatCount,
    SVGPropertyBase* fromValue,
    SVGPropertyBase* toValue,
    SVGPropertyBase* toAtEndOfDurationValue,
    SVGElement* contextElement) {
  SVGPointList* fromList = toSVGPointList(fromValue);
  SVGPointList* toList = toSVGPointList(toValue);
  SVGPointList* toAtEndOfDurationList = toSVGPointList(toAtEndOfDurationValue);

  size_t fromPointListSize = fromList->length();
  size_t toPointListSize = toList->length();
  size_t toAtEndOfDurationListSize = toAtEndOfDurationList->length();

  if (!adjustFromToListValues(fromList, toList, percentage,
                              animationElement->getAnimationMode()))
    return;

  for (size_t i = 0; i < toPointListSize; ++i) {
    float animatedX = at(i)->x();
    float animatedY = at(i)->y();

    FloatPoint effectiveFrom;
    if (fromPointListSize)
      effectiveFrom = fromList->at(i)->value();
    FloatPoint effectiveTo = toList->at(i)->value();
    FloatPoint effectiveToAtEnd;
    if (i < toAtEndOfDurationListSize)
      effectiveToAtEnd = toAtEndOfDurationList->at(i)->value();

    animationElement->animateAdditiveNumber(percentage, repeatCount,
                                            effectiveFrom.x(), effectiveTo.x(),
                                            effectiveToAtEnd.x(), animatedX);
    animationElement->animateAdditiveNumber(percentage, repeatCount,
                                            effectiveFrom.y(), effectiveTo.y(),
                                            effectiveToAtEnd.y(), animatedY);
    at(i)->setValue(FloatPoint(animatedX, animatedY));
  }
}

float SVGPointList::calculateDistance(SVGPropertyBase* to, SVGElement*) {
  // FIXME: Distance calculation is not possible for SVGPointList right now. We
  // need the distance for every single value.
  return -1;
}

}  // namespace blink
