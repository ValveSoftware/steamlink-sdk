/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/svg/SVGTransform.h"

#include "platform/FloatConversion.h"
#include "platform/geometry/FloatSize.h"
#include "wtf/MathExtras.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

SVGTransform::SVGTransform()
    : m_transformType(SVG_TRANSFORM_UNKNOWN)
    , m_angle(0)
{
}

SVGTransform::SVGTransform(SVGTransformType transformType, ConstructionMode mode)
    : m_transformType(transformType)
    , m_angle(0)
{
    if (mode == ConstructZeroTransform)
        m_matrix = AffineTransform(0, 0, 0, 0, 0, 0);
}

SVGTransform::SVGTransform(const AffineTransform& matrix)
    : m_transformType(SVG_TRANSFORM_MATRIX)
    , m_angle(0)
    , m_matrix(matrix)
{
}

SVGTransform::SVGTransform(SVGTransformType transformType, float angle, const FloatPoint& center, const AffineTransform& matrix)
    : m_transformType(transformType)
    , m_angle(angle)
    , m_center(center)
    , m_matrix(matrix)
{
}

SVGTransform::~SVGTransform()
{
}

SVGTransform* SVGTransform::clone() const
{
    return new SVGTransform(m_transformType, m_angle, m_center, m_matrix);
}

SVGPropertyBase* SVGTransform::cloneForAnimation(const String&) const
{
    // SVGTransform is never animated.
    ASSERT_NOT_REACHED();
    return nullptr;
}

void SVGTransform::setMatrix(const AffineTransform& matrix)
{
    onMatrixChange();
    m_matrix = matrix;
}

void SVGTransform::onMatrixChange()
{
    m_transformType = SVG_TRANSFORM_MATRIX;
    m_angle = 0;
}

void SVGTransform::setTranslate(float tx, float ty)
{
    m_transformType = SVG_TRANSFORM_TRANSLATE;
    m_angle = 0;

    m_matrix.makeIdentity();
    m_matrix.translate(tx, ty);
}

FloatPoint SVGTransform::translate() const
{
    return FloatPoint::narrowPrecision(m_matrix.e(), m_matrix.f());
}

void SVGTransform::setScale(float sx, float sy)
{
    m_transformType = SVG_TRANSFORM_SCALE;
    m_angle = 0;
    m_center = FloatPoint();

    m_matrix.makeIdentity();
    m_matrix.scaleNonUniform(sx, sy);
}

FloatSize SVGTransform::scale() const
{
    return FloatSize::narrowPrecision(m_matrix.a(), m_matrix.d());
}

void SVGTransform::setRotate(float angle, float cx, float cy)
{
    m_transformType = SVG_TRANSFORM_ROTATE;
    m_angle = angle;
    m_center = FloatPoint(cx, cy);

    // TODO: toString() implementation, which can show cx, cy (need to be stored?)
    m_matrix.makeIdentity();
    m_matrix.translate(cx, cy);
    m_matrix.rotate(angle);
    m_matrix.translate(-cx, -cy);
}

void SVGTransform::setSkewX(float angle)
{
    m_transformType = SVG_TRANSFORM_SKEWX;
    m_angle = angle;

    m_matrix.makeIdentity();
    m_matrix.skewX(angle);
}

void SVGTransform::setSkewY(float angle)
{
    m_transformType = SVG_TRANSFORM_SKEWY;
    m_angle = angle;

    m_matrix.makeIdentity();
    m_matrix.skewY(angle);
}

namespace {

const char* transformTypePrefixForParsing(SVGTransformType type)
{
    switch (type) {
    case SVG_TRANSFORM_UNKNOWN:
        return "";
    case SVG_TRANSFORM_MATRIX:
        return "matrix(";
    case SVG_TRANSFORM_TRANSLATE:
        return "translate(";
    case SVG_TRANSFORM_SCALE:
        return "scale(";
    case SVG_TRANSFORM_ROTATE:
        return "rotate(";
    case SVG_TRANSFORM_SKEWX:
        return "skewX(";
    case SVG_TRANSFORM_SKEWY:
        return "skewY(";
    }
    ASSERT_NOT_REACHED();
    return "";
}

} // namespace

String SVGTransform::valueAsString() const
{
    double arguments[6];
    size_t argumentCount = 0;
    switch (m_transformType) {
    case SVG_TRANSFORM_UNKNOWN:
        return emptyString();
    case SVG_TRANSFORM_MATRIX: {
        arguments[argumentCount++] = m_matrix.a();
        arguments[argumentCount++] = m_matrix.b();
        arguments[argumentCount++] = m_matrix.c();
        arguments[argumentCount++] = m_matrix.d();
        arguments[argumentCount++] = m_matrix.e();
        arguments[argumentCount++] = m_matrix.f();
        break;
    }
    case SVG_TRANSFORM_TRANSLATE: {
        arguments[argumentCount++] = m_matrix.e();
        arguments[argumentCount++] = m_matrix.f();
        break;
    }
    case SVG_TRANSFORM_SCALE: {
        arguments[argumentCount++] = m_matrix.a();
        arguments[argumentCount++] = m_matrix.d();
        break;
    }
    case SVG_TRANSFORM_ROTATE: {
        arguments[argumentCount++] = m_angle;

        double angleInRad = deg2rad(m_angle);
        double cosAngle = cos(angleInRad);
        double sinAngle = sin(angleInRad);
        float cx = narrowPrecisionToFloat(cosAngle != 1 ? (m_matrix.e() * (1 - cosAngle) - m_matrix.f() * sinAngle) / (1 - cosAngle) / 2 : 0);
        float cy = narrowPrecisionToFloat(cosAngle != 1 ? (m_matrix.e() * sinAngle / (1 - cosAngle) + m_matrix.f()) / 2 : 0);
        if (cx || cy) {
            arguments[argumentCount++] = cx;
            arguments[argumentCount++] = cy;
        }
        break;
    }
    case SVG_TRANSFORM_SKEWX:
        arguments[argumentCount++] = m_angle;
        break;
    case SVG_TRANSFORM_SKEWY:
        arguments[argumentCount++] = m_angle;
        break;
    }
    ASSERT(argumentCount <= WTF_ARRAY_LENGTH(arguments));

    StringBuilder builder;
    builder.append(transformTypePrefixForParsing(m_transformType));

    for (size_t i = 0; i < argumentCount; ++i) {
        if (i)
            builder.append(' ');
        builder.appendNumber(arguments[i]);
    }
    builder.append(')');
    return builder.toString();
}

void SVGTransform::add(SVGPropertyBase*, SVGElement*)
{
    // SVGTransform is not animated by itself.
    ASSERT_NOT_REACHED();
}

void SVGTransform::calculateAnimatedValue(SVGAnimationElement*, float, unsigned, SVGPropertyBase*, SVGPropertyBase*, SVGPropertyBase*, SVGElement*)
{
    // SVGTransform is not animated by itself.
    ASSERT_NOT_REACHED();
}

float SVGTransform::calculateDistance(SVGPropertyBase*, SVGElement*)
{
    // SVGTransform is not animated by itself.
    ASSERT_NOT_REACHED();

    return -1;
}

} // namespace blink
