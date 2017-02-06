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

#include "core/svg/SVGPoint.h"

#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

SVGPoint::SVGPoint()
{
}

SVGPoint::SVGPoint(const FloatPoint& point)
    : m_value(point)
{
}

SVGPoint* SVGPoint::clone() const
{
    return SVGPoint::create(m_value);
}

template<typename CharType>
SVGParsingError SVGPoint::parse(const CharType*& ptr, const CharType* end)
{
    float x = 0;
    float y = 0;
    if (!parseNumber(ptr, end, x)
        || !parseNumber(ptr, end, y, DisallowWhitespace))
        return SVGParseStatus::ExpectedNumber;

    if (skipOptionalSVGSpaces(ptr, end)) {
        // Nothing should come after the second number.
        return SVGParseStatus::TrailingGarbage;
    }

    m_value = FloatPoint(x, y);
    return SVGParseStatus::NoError;
}

FloatPoint SVGPoint::matrixTransform(const AffineTransform& transform) const
{
    double newX, newY;
    transform.map(static_cast<double>(x()), static_cast<double>(y()), newX, newY);
    return FloatPoint::narrowPrecision(newX, newY);
}

SVGParsingError SVGPoint::setValueAsString(const String& string)
{
    if (string.isEmpty()) {
        m_value = FloatPoint(0.0f, 0.0f);
        return SVGParseStatus::NoError;
    }

    if (string.is8Bit()) {
        const LChar* ptr = string.characters8();
        const LChar* end = ptr + string.length();
        return parse(ptr, end);
    }
    const UChar* ptr = string.characters16();
    const UChar* end = ptr + string.length();
    return parse(ptr, end);
}

String SVGPoint::valueAsString() const
{
    StringBuilder builder;
    builder.appendNumber(x());
    builder.append(' ');
    builder.appendNumber(y());
    return builder.toString();
}

void SVGPoint::add(SVGPropertyBase* other, SVGElement*)
{
    // SVGPoint is not animated by itself
    ASSERT_NOT_REACHED();
}

void SVGPoint::calculateAnimatedValue(SVGAnimationElement* animationElement, float percentage, unsigned repeatCount, SVGPropertyBase* fromValue, SVGPropertyBase* toValue, SVGPropertyBase* toAtEndOfDurationValue, SVGElement*)
{
    // SVGPoint is not animated by itself
    ASSERT_NOT_REACHED();
}

float SVGPoint::calculateDistance(SVGPropertyBase* to, SVGElement* contextElement)
{
    // SVGPoint is not animated by itself
    ASSERT_NOT_REACHED();
    return 0.0f;
}

} // namespace blink
