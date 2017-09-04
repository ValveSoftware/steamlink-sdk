/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGNumberList.h"

#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

SVGNumberList::SVGNumberList() {}

SVGNumberList::~SVGNumberList() {}

String SVGNumberList::valueAsString() const {
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
SVGParsingError SVGNumberList::parse(const CharType*& ptr,
                                     const CharType* end) {
  const CharType* listStart = ptr;
  while (ptr < end) {
    float number = 0;
    if (!parseNumber(ptr, end, number))
      return SVGParsingError(SVGParseStatus::ExpectedNumber, ptr - listStart);
    append(SVGNumber::create(number));
  }
  return SVGParseStatus::NoError;
}

SVGParsingError SVGNumberList::setValueAsString(const String& value) {
  clear();

  if (value.isEmpty())
    return SVGParseStatus::NoError;

  // Don't call |clear()| if an error is encountered. SVG policy is to use
  // valid items before error.
  // Spec: http://www.w3.org/TR/SVG/single-page.html#implnote-ErrorProcessing
  if (value.is8Bit()) {
    const LChar* ptr = value.characters8();
    const LChar* end = ptr + value.length();
    return parse(ptr, end);
  }
  const UChar* ptr = value.characters16();
  const UChar* end = ptr + value.length();
  return parse(ptr, end);
}

void SVGNumberList::add(SVGPropertyBase* other, SVGElement* contextElement) {
  SVGNumberList* otherList = toSVGNumberList(other);

  if (length() != otherList->length())
    return;

  for (size_t i = 0; i < length(); ++i)
    at(i)->setValue(at(i)->value() + otherList->at(i)->value());
}

void SVGNumberList::calculateAnimatedValue(
    SVGAnimationElement* animationElement,
    float percentage,
    unsigned repeatCount,
    SVGPropertyBase* fromValue,
    SVGPropertyBase* toValue,
    SVGPropertyBase* toAtEndOfDurationValue,
    SVGElement* contextElement) {
  SVGNumberList* fromList = toSVGNumberList(fromValue);
  SVGNumberList* toList = toSVGNumberList(toValue);
  SVGNumberList* toAtEndOfDurationList =
      toSVGNumberList(toAtEndOfDurationValue);

  size_t fromListSize = fromList->length();
  size_t toListSize = toList->length();
  size_t toAtEndOfDurationListSize = toAtEndOfDurationList->length();

  if (!adjustFromToListValues(fromList, toList, percentage,
                              animationElement->getAnimationMode()))
    return;

  for (size_t i = 0; i < toListSize; ++i) {
    float effectiveFrom = fromListSize ? fromList->at(i)->value() : 0;
    float effectiveTo = toListSize ? toList->at(i)->value() : 0;
    float effectiveToAtEnd = i < toAtEndOfDurationListSize
                                 ? toAtEndOfDurationList->at(i)->value()
                                 : 0;

    float animated = at(i)->value();
    animationElement->animateAdditiveNumber(percentage, repeatCount,
                                            effectiveFrom, effectiveTo,
                                            effectiveToAtEnd, animated);
    at(i)->setValue(animated);
  }
}

float SVGNumberList::calculateDistance(SVGPropertyBase* to, SVGElement*) {
  // FIXME: Distance calculation is not possible for SVGNumberList right now. We
  // need the distance for every single value.
  return -1;
}

Vector<float> SVGNumberList::toFloatVector() const {
  Vector<float> vec;
  vec.reserveInitialCapacity(length());
  for (size_t i = 0; i < length(); ++i)
    vec.uncheckedAppend(at(i)->value());
  return vec;
}

}  // namespace blink
