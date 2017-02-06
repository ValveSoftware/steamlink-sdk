/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/css/CSSBasicShapeValues.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValuePair.h"
#include "platform/Length.h"
#include "wtf/text/StringBuilder.h"

using namespace WTF;

namespace blink {

static String buildCircleString(const String& radius, const String& centerX, const String& centerY)
{
    char at[] = "at";
    char separator[] = " ";
    StringBuilder result;
    result.append("circle(");
    if (!radius.isNull())
        result.append(radius);

    if (!centerX.isNull() || !centerY.isNull()) {
        if (!radius.isNull())
            result.append(separator);
        result.append(at);
        result.append(separator);
        result.append(centerX);
        result.append(separator);
        result.append(centerY);
    }
    result.append(')');
    return result.toString();
}

static String serializePositionOffset(const CSSValuePair& offset, const CSSValuePair& other)
{
    if ((toCSSPrimitiveValue(offset.first()).getValueID() == CSSValueLeft && toCSSPrimitiveValue(other.first()).getValueID() == CSSValueTop)
        || (toCSSPrimitiveValue(offset.first()).getValueID() == CSSValueTop && toCSSPrimitiveValue(other.first()).getValueID() == CSSValueLeft))
        return offset.second().cssText();
    return offset.cssText();
}

static CSSValuePair* buildSerializablePositionOffset(CSSValue* offset, CSSValueID defaultSide)
{
    CSSValueID side = defaultSide;
    CSSPrimitiveValue* amount = nullptr;

    if (!offset) {
        side = CSSValueCenter;
    } else if (offset->isPrimitiveValue() && toCSSPrimitiveValue(offset)->isValueID()) {
        side = toCSSPrimitiveValue(offset)->getValueID();
    } else if (offset->isValuePair()) {
        side = toCSSPrimitiveValue(toCSSValuePair(*offset).first()).getValueID();
        amount = &toCSSPrimitiveValue(toCSSValuePair(*offset).second());
        if ((side == CSSValueRight || side == CSSValueBottom) && amount->isPercentage()) {
            side = defaultSide;
            amount = CSSPrimitiveValue::create(100 - amount->getFloatValue(), CSSPrimitiveValue::UnitType::Percentage);
        }
    } else {
        amount = toCSSPrimitiveValue(offset);
    }

    if (side == CSSValueCenter) {
        side = defaultSide;
        amount = CSSPrimitiveValue::create(50, CSSPrimitiveValue::UnitType::Percentage);
    } else if (!amount || (amount->isLength() && !amount->getFloatValue())) {
        if (side == CSSValueRight || side == CSSValueBottom)
            amount = CSSPrimitiveValue::create(100, CSSPrimitiveValue::UnitType::Percentage);
        else
            amount = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Percentage);
        side = defaultSide;
    }

    return CSSValuePair::create(CSSPrimitiveValue::createIdentifier(side), amount, CSSValuePair::KeepIdenticalValues);
}

String CSSBasicShapeCircleValue::customCSSText() const
{
    CSSValuePair* normalizedCX = buildSerializablePositionOffset(m_centerX, CSSValueLeft);
    CSSValuePair* normalizedCY = buildSerializablePositionOffset(m_centerY, CSSValueTop);

    String radius;
    if (m_radius && m_radius->getValueID() != CSSValueClosestSide)
        radius = m_radius->cssText();

    return buildCircleString(radius,
        serializePositionOffset(*normalizedCX, *normalizedCY),
        serializePositionOffset(*normalizedCY, *normalizedCX));
}

bool CSSBasicShapeCircleValue::equals(const CSSBasicShapeCircleValue& other) const
{
    return compareCSSValuePtr(m_centerX, other.m_centerX)
        && compareCSSValuePtr(m_centerY, other.m_centerY)
        && compareCSSValuePtr(m_radius, other.m_radius);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSBasicShapeCircleValue)
{
    visitor->trace(m_centerX);
    visitor->trace(m_centerY);
    visitor->trace(m_radius);
    CSSValue::traceAfterDispatch(visitor);
}

static String buildEllipseString(const String& radiusX, const String& radiusY, const String& centerX, const String& centerY)
{
    char at[] = "at";
    char separator[] = " ";
    StringBuilder result;
    result.append("ellipse(");
    bool needsSeparator = false;
    if (!radiusX.isNull()) {
        result.append(radiusX);
        needsSeparator = true;
    }
    if (!radiusY.isNull()) {
        if (needsSeparator)
            result.append(separator);
        result.append(radiusY);
        needsSeparator = true;
    }

    if (!centerX.isNull() || !centerY.isNull()) {
        if (needsSeparator)
            result.append(separator);
        result.append(at);
        result.append(separator);
        result.append(centerX);
        result.append(separator);
        result.append(centerY);
    }
    result.append(')');
    return result.toString();
}

String CSSBasicShapeEllipseValue::customCSSText() const
{
    CSSValuePair* normalizedCX = buildSerializablePositionOffset(m_centerX, CSSValueLeft);
    CSSValuePair* normalizedCY = buildSerializablePositionOffset(m_centerY, CSSValueTop);

    String radiusX;
    String radiusY;
    if (m_radiusX) {
        bool shouldSerializeRadiusXValue = m_radiusX->getValueID() != CSSValueClosestSide;
        bool shouldSerializeRadiusYValue = false;

        if (m_radiusY) {
            shouldSerializeRadiusYValue = m_radiusY->getValueID() != CSSValueClosestSide;
            if (shouldSerializeRadiusYValue)
                radiusY = m_radiusY->cssText();
        }
        if (shouldSerializeRadiusXValue || (!shouldSerializeRadiusXValue && shouldSerializeRadiusYValue))
            radiusX = m_radiusX->cssText();
    }

    return buildEllipseString(radiusX, radiusY,
        serializePositionOffset(*normalizedCX, *normalizedCY),
        serializePositionOffset(*normalizedCY, *normalizedCX));
}

bool CSSBasicShapeEllipseValue::equals(const CSSBasicShapeEllipseValue& other) const
{
    return compareCSSValuePtr(m_centerX, other.m_centerX)
        && compareCSSValuePtr(m_centerY, other.m_centerY)
        && compareCSSValuePtr(m_radiusX, other.m_radiusX)
        && compareCSSValuePtr(m_radiusY, other.m_radiusY);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSBasicShapeEllipseValue)
{
    visitor->trace(m_centerX);
    visitor->trace(m_centerY);
    visitor->trace(m_radiusX);
    visitor->trace(m_radiusY);
    CSSValue::traceAfterDispatch(visitor);
}

static String buildPolygonString(const WindRule& windRule, const Vector<String>& points)
{
    ASSERT(!(points.size() % 2));

    StringBuilder result;
    const char evenOddOpening[] = "polygon(evenodd, ";
    const char nonZeroOpening[] = "polygon(";
    const char commaSeparator[] = ", ";
    static_assert(sizeof(evenOddOpening) > sizeof(nonZeroOpening), "polygon string openings should be the same length");

    // Compute the required capacity in advance to reduce allocations.
    size_t length = sizeof(evenOddOpening) - 1;
    for (size_t i = 0; i < points.size(); i += 2) {
        if (i)
            length += (sizeof(commaSeparator) - 1);
        // add length of two strings, plus one for the space separator.
        length += points[i].length() + 1 + points[i + 1].length();
    }
    result.reserveCapacity(length);

    if (windRule == RULE_EVENODD)
        result.append(evenOddOpening);
    else
        result.append(nonZeroOpening);

    for (size_t i = 0; i < points.size(); i += 2) {
        if (i)
            result.append(commaSeparator);
        result.append(points[i]);
        result.append(' ');
        result.append(points[i + 1]);
    }

    result.append(')');
    return result.toString();
}

String CSSBasicShapePolygonValue::customCSSText() const
{
    Vector<String> points;
    points.reserveInitialCapacity(m_values.size());

    for (size_t i = 0; i < m_values.size(); ++i)
        points.append(m_values.at(i)->cssText());

    return buildPolygonString(m_windRule, points);
}

bool CSSBasicShapePolygonValue::equals(const CSSBasicShapePolygonValue& other) const
{
    return compareCSSValueVector(m_values, other.m_values);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSBasicShapePolygonValue)
{
    visitor->trace(m_values);
    CSSValue::traceAfterDispatch(visitor);
}

static bool buildInsetRadii(Vector<String> &radii, const String& topLeftRadius, const String& topRightRadius, const String& bottomRightRadius, const String& bottomLeftRadius)
{
    bool showBottomLeft = topRightRadius != bottomLeftRadius;
    bool showBottomRight = showBottomLeft || (bottomRightRadius != topLeftRadius);
    bool showTopRight = showBottomRight || (topRightRadius != topLeftRadius);

    radii.append(topLeftRadius);
    if (showTopRight)
        radii.append(topRightRadius);
    if (showBottomRight)
        radii.append(bottomRightRadius);
    if (showBottomLeft)
        radii.append(bottomLeftRadius);

    return radii.size() == 1 && radii[0] == "0px";
}

static String buildInsetString(const String& top, const String& right, const String& bottom, const String& left,
    const String& topLeftRadiusWidth, const String& topLeftRadiusHeight,
    const String& topRightRadiusWidth, const String& topRightRadiusHeight,
    const String& bottomRightRadiusWidth, const String& bottomRightRadiusHeight,
    const String& bottomLeftRadiusWidth, const String& bottomLeftRadiusHeight)
{
    char opening[] = "inset(";
    char separator[] = " ";
    char cornersSeparator[] = "round";
    StringBuilder result;
    result.append(opening);
    result.append(top);
    bool showLeftArg = !left.isNull() && left != right;
    bool showBottomArg = !bottom.isNull() && (bottom != top || showLeftArg);
    bool showRightArg = !right.isNull() && (right != top || showBottomArg);
    if (showRightArg) {
        result.append(separator);
        result.append(right);
    }
    if (showBottomArg) {
        result.append(separator);
        result.append(bottom);
    }
    if (showLeftArg) {
        result.append(separator);
        result.append(left);
    }

    if (!topLeftRadiusWidth.isNull() && !topLeftRadiusHeight.isNull()) {
        Vector<String> horizontalRadii;
        bool areDefaultCornerRadii = buildInsetRadii(horizontalRadii, topLeftRadiusWidth, topRightRadiusWidth, bottomRightRadiusWidth, bottomLeftRadiusWidth);

        Vector<String> verticalRadii;
        areDefaultCornerRadii &= buildInsetRadii(verticalRadii, topLeftRadiusHeight, topRightRadiusHeight, bottomRightRadiusHeight, bottomLeftRadiusHeight);

        if (!areDefaultCornerRadii) {
            result.append(separator);
            result.append(cornersSeparator);

            for (size_t i = 0; i < horizontalRadii.size(); ++i) {
                result.append(separator);
                result.append(horizontalRadii[i]);
            }
            if (horizontalRadii != verticalRadii) {
                result.append(separator);
                result.append("/");

                for (size_t i = 0; i < verticalRadii.size(); ++i) {
                    result.append(separator);
                    result.append(verticalRadii[i]);
                }
            }
        }
    }
    result.append(')');

    return result.toString();
}

static inline void updateCornerRadiusWidthAndHeight(const CSSValuePair* cornerRadius, String& width, String& height)
{
    if (!cornerRadius)
        return;

    width = cornerRadius->first().cssText();
    height = cornerRadius->second().cssText();
}

String CSSBasicShapeInsetValue::customCSSText() const
{
    String topLeftRadiusWidth;
    String topLeftRadiusHeight;
    String topRightRadiusWidth;
    String topRightRadiusHeight;
    String bottomRightRadiusWidth;
    String bottomRightRadiusHeight;
    String bottomLeftRadiusWidth;
    String bottomLeftRadiusHeight;

    updateCornerRadiusWidthAndHeight(topLeftRadius(), topLeftRadiusWidth, topLeftRadiusHeight);
    updateCornerRadiusWidthAndHeight(topRightRadius(), topRightRadiusWidth, topRightRadiusHeight);
    updateCornerRadiusWidthAndHeight(bottomRightRadius(), bottomRightRadiusWidth, bottomRightRadiusHeight);
    updateCornerRadiusWidthAndHeight(bottomLeftRadius(), bottomLeftRadiusWidth, bottomLeftRadiusHeight);

    return buildInsetString(m_top ? m_top->cssText() : String(),
        m_right ? m_right->cssText() : String(),
        m_bottom ? m_bottom->cssText() : String(),
        m_left ? m_left->cssText() : String(),
        topLeftRadiusWidth,
        topLeftRadiusHeight,
        topRightRadiusWidth,
        topRightRadiusHeight,
        bottomRightRadiusWidth,
        bottomRightRadiusHeight,
        bottomLeftRadiusWidth,
        bottomLeftRadiusHeight);
}

bool CSSBasicShapeInsetValue::equals(const CSSBasicShapeInsetValue& other) const
{
    return compareCSSValuePtr(m_top, other.m_top)
        && compareCSSValuePtr(m_right, other.m_right)
        && compareCSSValuePtr(m_bottom, other.m_bottom)
        && compareCSSValuePtr(m_left, other.m_left)
        && compareCSSValuePtr(m_topLeftRadius, other.m_topLeftRadius)
        && compareCSSValuePtr(m_topRightRadius, other.m_topRightRadius)
        && compareCSSValuePtr(m_bottomRightRadius, other.m_bottomRightRadius)
        && compareCSSValuePtr(m_bottomLeftRadius, other.m_bottomLeftRadius);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSBasicShapeInsetValue)
{
    visitor->trace(m_top);
    visitor->trace(m_right);
    visitor->trace(m_bottom);
    visitor->trace(m_left);
    visitor->trace(m_topLeftRadius);
    visitor->trace(m_topRightRadius);
    visitor->trace(m_bottomRightRadius);
    visitor->trace(m_bottomLeftRadius);
    CSSValue::traceAfterDispatch(visitor);
}

} // namespace blink

