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

#include "core/svg/SVGLengthList.h"

#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

SVGLengthList::SVGLengthList(SVGLengthMode mode) : m_mode(mode) {}

SVGLengthList::~SVGLengthList() {}

SVGLengthList* SVGLengthList::clone() {
  SVGLengthList* ret = SVGLengthList::create(m_mode);
  ret->deepCopy(this);
  return ret;
}

SVGPropertyBase* SVGLengthList::cloneForAnimation(const String& value) const {
  SVGLengthList* ret = SVGLengthList::create(m_mode);
  ret->setValueAsString(value);
  return ret;
}

String SVGLengthList::valueAsString() const {
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
SVGParsingError SVGLengthList::parseInternal(const CharType*& ptr,
                                             const CharType* end) {
  const CharType* listStart = ptr;
  while (ptr < end) {
    const CharType* start = ptr;
    // TODO(shanmuga.m): Enable calc for SVGLengthList
    while (ptr < end && *ptr != ',' && !isHTMLSpace<CharType>(*ptr))
      ptr++;
    if (ptr == start)
      break;
    String valueString(start, ptr - start);
    if (valueString.isEmpty())
      break;

    SVGLength* length = SVGLength::create(m_mode);
    SVGParsingError lengthParseStatus = length->setValueAsString(valueString);
    if (lengthParseStatus != SVGParseStatus::NoError)
      return lengthParseStatus.offsetWith(start - listStart);
    append(length);
    skipOptionalSVGSpacesOrDelimiter(ptr, end);
  }
  return SVGParseStatus::NoError;
}

SVGParsingError SVGLengthList::setValueAsString(const String& value) {
  clear();

  if (value.isEmpty())
    return SVGParseStatus::NoError;

  if (value.is8Bit()) {
    const LChar* ptr = value.characters8();
    const LChar* end = ptr + value.length();
    return parseInternal(ptr, end);
  }
  const UChar* ptr = value.characters16();
  const UChar* end = ptr + value.length();
  return parseInternal(ptr, end);
}

void SVGLengthList::add(SVGPropertyBase* other, SVGElement* contextElement) {
  SVGLengthList* otherList = toSVGLengthList(other);

  if (length() != otherList->length())
    return;

  SVGLengthContext lengthContext(contextElement);
  for (size_t i = 0; i < length(); ++i)
    at(i)->setValue(
        at(i)->value(lengthContext) + otherList->at(i)->value(lengthContext),
        lengthContext);
}

SVGLength* SVGLengthList::createPaddingItem() const {
  return SVGLength::create(m_mode);
}

void SVGLengthList::calculateAnimatedValue(
    SVGAnimationElement* animationElement,
    float percentage,
    unsigned repeatCount,
    SVGPropertyBase* fromValue,
    SVGPropertyBase* toValue,
    SVGPropertyBase* toAtEndOfDurationValue,
    SVGElement* contextElement) {
  SVGLengthList* fromList = toSVGLengthList(fromValue);
  SVGLengthList* toList = toSVGLengthList(toValue);
  SVGLengthList* toAtEndOfDurationList =
      toSVGLengthList(toAtEndOfDurationValue);

  SVGLengthContext lengthContext(contextElement);
  ASSERT(m_mode == SVGLength::lengthModeForAnimatedLengthAttribute(
                       animationElement->attributeName()));

  size_t fromLengthListSize = fromList->length();
  size_t toLengthListSize = toList->length();
  size_t toAtEndOfDurationListSize = toAtEndOfDurationList->length();

  if (!adjustFromToListValues(fromList, toList, percentage,
                              animationElement->getAnimationMode()))
    return;

  for (size_t i = 0; i < toLengthListSize; ++i) {
    // TODO(shanmuga.m): Support calc for SVGLengthList animation
    float animatedNumber = at(i)->value(lengthContext);
    CSSPrimitiveValue::UnitType unitType =
        toList->at(i)->typeWithCalcResolved();
    float effectiveFrom = 0;
    if (fromLengthListSize) {
      if (percentage < 0.5)
        unitType = fromList->at(i)->typeWithCalcResolved();
      effectiveFrom = fromList->at(i)->value(lengthContext);
    }
    float effectiveTo = toList->at(i)->value(lengthContext);
    float effectiveToAtEnd =
        i < toAtEndOfDurationListSize
            ? toAtEndOfDurationList->at(i)->value(lengthContext)
            : 0;

    animationElement->animateAdditiveNumber(percentage, repeatCount,
                                            effectiveFrom, effectiveTo,
                                            effectiveToAtEnd, animatedNumber);
    at(i)->setUnitType(unitType);
    at(i)->setValue(animatedNumber, lengthContext);
  }
}

float SVGLengthList::calculateDistance(SVGPropertyBase* to, SVGElement*) {
  // FIXME: Distance calculation is not possible for SVGLengthList right now. We
  // need the distance for every single value.
  return -1;
}
}  // namespace blink
