/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2012. All rights reserved.
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

#include "core/svg/SVGTransformList.h"

#include "core/SVGNames.h"
#include "core/svg/SVGParserUtilities.h"
#include "core/svg/SVGTransformDistance.h"
#include "platform/text/ParserUtilities.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

SVGTransformList::SVGTransformList()
{
}

SVGTransformList::~SVGTransformList()
{
}

SVGTransform* SVGTransformList::consolidate()
{
    AffineTransform matrix;
    if (!concatenate(matrix))
        return SVGTransform::create();

    return initialize(SVGTransform::create(matrix));
}

bool SVGTransformList::concatenate(AffineTransform& result) const
{
    if (isEmpty())
        return false;

    ConstIterator it = begin();
    ConstIterator itEnd = end();
    for (; it != itEnd; ++it)
        result *= it->matrix();

    return true;
}

namespace {

const LChar skewXDesc[] =  {'s', 'k', 'e', 'w', 'X'};
const LChar skewYDesc[] =  {'s', 'k', 'e', 'w', 'Y'};
const LChar scaleDesc[] =  {'s', 'c', 'a', 'l', 'e'};
const LChar translateDesc[] =  {'t', 'r', 'a', 'n', 's', 'l', 'a', 't', 'e'};
const LChar rotateDesc[] =  {'r', 'o', 't', 'a', 't', 'e'};
const LChar matrixDesc[] =  {'m', 'a', 't', 'r', 'i', 'x'};

template<typename CharType>
SVGTransformType parseAndSkipTransformType(const CharType*& ptr, const CharType* end)
{
    if (ptr >= end)
        return SVG_TRANSFORM_UNKNOWN;

    if (*ptr == 's') {
        if (skipString(ptr, end, skewXDesc, WTF_ARRAY_LENGTH(skewXDesc)))
            return SVG_TRANSFORM_SKEWX;
        if (skipString(ptr, end, skewYDesc, WTF_ARRAY_LENGTH(skewYDesc)))
            return SVG_TRANSFORM_SKEWY;
        if (skipString(ptr, end, scaleDesc, WTF_ARRAY_LENGTH(scaleDesc)))
            return SVG_TRANSFORM_SCALE;

        return SVG_TRANSFORM_UNKNOWN;
    }
    if (skipString(ptr, end, translateDesc, WTF_ARRAY_LENGTH(translateDesc)))
        return SVG_TRANSFORM_TRANSLATE;
    if (skipString(ptr, end, rotateDesc, WTF_ARRAY_LENGTH(rotateDesc)))
        return SVG_TRANSFORM_ROTATE;
    if (skipString(ptr, end, matrixDesc, WTF_ARRAY_LENGTH(matrixDesc)))
        return SVG_TRANSFORM_MATRIX;

    return SVG_TRANSFORM_UNKNOWN;
}

// These should be kept in sync with enum SVGTransformType
const unsigned requiredValuesForType[] =  {0, 6, 1, 1, 1, 1, 1};
const unsigned optionalValuesForType[] =  {0, 0, 1, 1, 2, 0, 0};
static_assert(SVG_TRANSFORM_UNKNOWN == 0, "index of SVG_TRANSFORM_UNKNOWN has changed");
static_assert(SVG_TRANSFORM_MATRIX == 1, "index of SVG_TRANSFORM_MATRIX has changed");
static_assert(SVG_TRANSFORM_TRANSLATE == 2, "index of SVG_TRANSFORM_TRANSLATE has changed");
static_assert(SVG_TRANSFORM_SCALE == 3, "index of SVG_TRANSFORM_SCALE has changed");
static_assert(SVG_TRANSFORM_ROTATE == 4, "index of SVG_TRANSFORM_ROTATE has changed");
static_assert(SVG_TRANSFORM_SKEWX == 5, "index of SVG_TRANSFORM_SKEWX has changed");
static_assert(SVG_TRANSFORM_SKEWY == 6, "index of SVG_TRANSFORM_SKEWY has changed");
static_assert(WTF_ARRAY_LENGTH(requiredValuesForType) - 1 == SVG_TRANSFORM_SKEWY, "the number of transform types have changed");
static_assert(WTF_ARRAY_LENGTH(requiredValuesForType) == WTF_ARRAY_LENGTH(optionalValuesForType), "the arrays should have the same number of elements");

const unsigned kMaxTransformArguments = 6;

using TransformArguments = Vector<float, kMaxTransformArguments>;

template<typename CharType>
SVGParseStatus parseTransformArgumentsForType(
    SVGTransformType type,
    const CharType*& ptr, const CharType* end,
    TransformArguments& arguments)
{
    const size_t required = requiredValuesForType[type];
    const size_t optional = optionalValuesForType[type];
    const size_t requiredWithOptional = required + optional;
    ASSERT(requiredWithOptional <= kMaxTransformArguments);
    ASSERT(arguments.isEmpty());

    bool trailingDelimiter = false;

    while (arguments.size() < requiredWithOptional) {
        float argumentValue = 0;
        if (!parseNumber(ptr, end, argumentValue, AllowLeadingWhitespace))
            break;

        arguments.append(argumentValue);
        trailingDelimiter = false;

        if (arguments.size() == requiredWithOptional)
            break;

        if (skipOptionalSVGSpaces(ptr, end) && *ptr == ',') {
            ++ptr;
            trailingDelimiter = true;
        }
    }

    if (arguments.size() != required && arguments.size() != requiredWithOptional)
        return SVGParseStatus::ExpectedNumber;
    if (trailingDelimiter)
        return SVGParseStatus::TrailingGarbage;

    return SVGParseStatus::NoError;
}

SVGTransform* createTransformFromValues(SVGTransformType type, const TransformArguments& arguments)
{
    SVGTransform* transform = SVGTransform::create();
    switch (type) {
    case SVG_TRANSFORM_SKEWX:
        transform->setSkewX(arguments[0]);
        break;
    case SVG_TRANSFORM_SKEWY:
        transform->setSkewY(arguments[0]);
        break;
    case SVG_TRANSFORM_SCALE:
        // Spec: if only one param given, assume uniform scaling.
        if (arguments.size() == 1)
            transform->setScale(arguments[0], arguments[0]);
        else
            transform->setScale(arguments[0], arguments[1]);
        break;
    case SVG_TRANSFORM_TRANSLATE:
        // Spec: if only one param given, assume 2nd param to be 0.
        if (arguments.size() == 1)
            transform->setTranslate(arguments[0], 0);
        else
            transform->setTranslate(arguments[0], arguments[1]);
        break;
    case SVG_TRANSFORM_ROTATE:
        if (arguments.size() == 1)
            transform->setRotate(arguments[0], 0, 0);
        else
            transform->setRotate(arguments[0], arguments[1], arguments[2]);
        break;
    case SVG_TRANSFORM_MATRIX:
        transform->setMatrix(AffineTransform(arguments[0], arguments[1], arguments[2], arguments[3], arguments[4], arguments[5]));
        break;
    case SVG_TRANSFORM_UNKNOWN:
        ASSERT_NOT_REACHED();
        break;
    }
    return transform;
}

} // namespace

template<typename CharType>
SVGParsingError SVGTransformList::parseInternal(const CharType*& ptr, const CharType* end)
{
    clear();

    const CharType* start = ptr;
    bool delimParsed = false;
    while (skipOptionalSVGSpaces(ptr, end)) {
        delimParsed = false;

        SVGTransformType transformType = parseAndSkipTransformType(ptr, end);
        if (transformType == SVG_TRANSFORM_UNKNOWN)
            return SVGParsingError(SVGParseStatus::ExpectedTransformFunction, ptr - start);

        if (!skipOptionalSVGSpaces(ptr, end) || *ptr != '(')
            return SVGParsingError(SVGParseStatus::ExpectedStartOfArguments, ptr - start);
        ptr++;

        TransformArguments arguments;
        SVGParseStatus status = parseTransformArgumentsForType(transformType, ptr, end, arguments);
        if (status != SVGParseStatus::NoError)
            return SVGParsingError(status, ptr - start);
        ASSERT(arguments.size() >= requiredValuesForType[transformType]);

        if (!skipOptionalSVGSpaces(ptr, end) || *ptr != ')')
            return SVGParsingError(SVGParseStatus::ExpectedEndOfArguments, ptr - start);
        ptr++;

        append(createTransformFromValues(transformType, arguments));

        if (skipOptionalSVGSpaces(ptr, end) && *ptr == ',') {
            ++ptr;
            delimParsed = true;
        }
    }
    if (delimParsed)
        return SVGParsingError(SVGParseStatus::TrailingGarbage, ptr - start);
    return SVGParseStatus::NoError;
}

bool SVGTransformList::parse(const UChar*& ptr, const UChar* end)
{
    return parseInternal(ptr, end) == SVGParseStatus::NoError;
}

bool SVGTransformList::parse(const LChar*& ptr, const LChar* end)
{
    return parseInternal(ptr, end) == SVGParseStatus::NoError;
}

SVGTransformType parseTransformType(const String& string)
{
    if (string.isEmpty())
        return SVG_TRANSFORM_UNKNOWN;
    if (string.is8Bit()) {
        const LChar* ptr = string.characters8();
        const LChar* end = ptr + string.length();
        return parseAndSkipTransformType(ptr, end);
    }
    const UChar* ptr = string.characters16();
    const UChar* end = ptr + string.length();
    return parseAndSkipTransformType(ptr, end);
}

String SVGTransformList::valueAsString() const
{
    StringBuilder builder;

    ConstIterator it = begin();
    ConstIterator itEnd = end();
    while (it != itEnd) {
        builder.append(it->valueAsString());
        ++it;
        if (it != itEnd)
            builder.append(' ');
    }

    return builder.toString();
}

SVGParsingError SVGTransformList::setValueAsString(const String& value)
{
    if (value.isEmpty()) {
        clear();
        return SVGParseStatus::NoError;
    }

    SVGParsingError parseError;
    if (value.is8Bit()) {
        const LChar* ptr = value.characters8();
        const LChar* end = ptr + value.length();
        parseError = parseInternal(ptr, end);
    } else {
        const UChar* ptr = value.characters16();
        const UChar* end = ptr + value.length();
        parseError = parseInternal(ptr, end);
    }

    if (parseError != SVGParseStatus::NoError)
        clear();

    return parseError;
}

SVGPropertyBase* SVGTransformList::cloneForAnimation(const String& value) const
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsSVGEnabled());
    return SVGListPropertyHelper::cloneForAnimation(value);
}

SVGTransformList* SVGTransformList::create(SVGTransformType transformType, const String& value)
{
    TransformArguments arguments;
    bool atEndOfValue = false;
    SVGParseStatus status = SVGParseStatus::ParsingFailed;
    if (value.isEmpty()) {
    } else if (value.is8Bit()) {
        const LChar* ptr = value.characters8();
        const LChar* end = ptr + value.length();
        status = parseTransformArgumentsForType(transformType, ptr, end, arguments);
        atEndOfValue = !skipOptionalSVGSpaces(ptr, end);
    } else {
        const UChar* ptr = value.characters16();
        const UChar* end = ptr + value.length();
        status = parseTransformArgumentsForType(transformType, ptr, end, arguments);
        atEndOfValue = !skipOptionalSVGSpaces(ptr, end);
    }

    SVGTransformList* svgTransformList = SVGTransformList::create();
    if (atEndOfValue && status == SVGParseStatus::NoError)
        svgTransformList->append(createTransformFromValues(transformType, arguments));
    return svgTransformList;
}

void SVGTransformList::add(SVGPropertyBase* other, SVGElement* contextElement)
{
    if (isEmpty())
        return;

    SVGTransformList* otherList = toSVGTransformList(other);
    if (length() != otherList->length())
        return;

    ASSERT(length() == 1);
    SVGTransform* fromTransform = at(0);
    SVGTransform* toTransform = otherList->at(0);

    ASSERT(fromTransform->transformType() == toTransform->transformType());
    initialize(SVGTransformDistance::addSVGTransforms(fromTransform, toTransform));
}

void SVGTransformList::calculateAnimatedValue(SVGAnimationElement* animationElement, float percentage, unsigned repeatCount, SVGPropertyBase* fromValue, SVGPropertyBase* toValue, SVGPropertyBase* toAtEndOfDurationValue, SVGElement* contextElement)
{
    ASSERT(animationElement);
    bool isToAnimation = animationElement->getAnimationMode() == ToAnimation;

    // Spec: To animations provide specific functionality to get a smooth change from the underlying value to the
    // 'to' attribute value, which conflicts mathematically with the requirement for additive transform animations
    // to be post-multiplied. As a consequence, in SVG 1.1 the behavior of to animations for 'animateTransform' is undefined
    // FIXME: This is not taken into account yet.
    SVGTransformList* fromList = isToAnimation ? this : toSVGTransformList(fromValue);
    SVGTransformList* toList = toSVGTransformList(toValue);
    SVGTransformList* toAtEndOfDurationList = toSVGTransformList(toAtEndOfDurationValue);

    size_t toListSize = toList->length();
    if (!toListSize)
        return;

    // Get a reference to the from value before potentially cleaning it out (in the case of a To animation.)
    SVGTransform* toTransform = toList->at(0);
    SVGTransform* effectiveFrom = nullptr;
    // If there's an existing 'from'/underlying value of the same type use that, else use a "zero transform".
    if (fromList->length() && fromList->at(0)->transformType() == toTransform->transformType())
        effectiveFrom = fromList->at(0);
    else
        effectiveFrom = SVGTransform::create(toTransform->transformType(), SVGTransform::ConstructZeroTransform);

    // Never resize the animatedTransformList to the toList size, instead either clear the list or append to it.
    if (!isEmpty() && (!animationElement->isAdditive() || isToAnimation))
        clear();

    SVGTransform* currentTransform = SVGTransformDistance(effectiveFrom, toTransform).scaledDistance(percentage).addToSVGTransform(effectiveFrom);
    if (animationElement->isAccumulated() && repeatCount) {
        SVGTransform* effectiveToAtEnd = !toAtEndOfDurationList->isEmpty() ? toAtEndOfDurationList->at(0) : SVGTransform::create(toTransform->transformType(), SVGTransform::ConstructZeroTransform);
        append(SVGTransformDistance::addSVGTransforms(currentTransform, effectiveToAtEnd, repeatCount));
    } else {
        append(currentTransform);
    }
}

float SVGTransformList::calculateDistance(SVGPropertyBase* toValue, SVGElement*)
{
    // FIXME: This is not correct in all cases. The spec demands that each component (translate x and y for example)
    // is paced separately. To implement this we need to treat each component as individual animation everywhere.

    SVGTransformList* toList = toSVGTransformList(toValue);
    if (isEmpty() || length() != toList->length())
        return -1;

    ASSERT(length() == 1);
    if (at(0)->transformType() == toList->at(0)->transformType())
        return -1;

    // Spec: http://www.w3.org/TR/SVG/animate.html#complexDistances
    // Paced animations assume a notion of distance between the various animation values defined by the 'to', 'from', 'by' and 'values' attributes.
    // Distance is defined only for scalar types (such as <length>), colors and the subset of transformation types that are supported by 'animateTransform'.
    return SVGTransformDistance(at(0), toList->at(0)).distance();
}

} // namespace blink
