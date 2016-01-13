/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#include "config.h"
#include "core/svg/SVGPathStringBuilder.h"

#include "wtf/text/WTFString.h"

namespace WebCore {

String SVGPathStringBuilder::result()
{
    unsigned size = m_stringBuilder.length();
    if (!size)
        return String();

    // Remove trailing space.
    m_stringBuilder.resize(size - 1);
    return m_stringBuilder.toString();
}

static void appendFloat(StringBuilder& stringBuilder, float value)
{
    stringBuilder.append(' ');
    stringBuilder.appendNumber(value);
}

static void appendBool(StringBuilder& stringBuilder, bool value)
{
    stringBuilder.append(' ');
    stringBuilder.appendNumber(value);
}

static void appendPoint(StringBuilder& stringBuilder, const FloatPoint& point)
{
    appendFloat(stringBuilder, point.x());
    appendFloat(stringBuilder, point.y());
}

static void emitCommand1Arg(StringBuilder& stringBuilder, char commandChar, float argument)
{
    stringBuilder.append(commandChar);
    appendFloat(stringBuilder, argument);
    stringBuilder.append(' ');
}

static void emitCommand1Arg(StringBuilder& stringBuilder, char commandChar, const FloatPoint& argumentPoint)
{
    stringBuilder.append(commandChar);
    appendPoint(stringBuilder, argumentPoint);
    stringBuilder.append(' ');
}

static void emitCommand2Arg(StringBuilder& stringBuilder, char commandChar, const FloatPoint& argument1Point, const FloatPoint& argument2Point)
{
    stringBuilder.append(commandChar);
    appendPoint(stringBuilder, argument1Point);
    appendPoint(stringBuilder, argument2Point);
    stringBuilder.append(' ');
}

void SVGPathStringBuilder::moveTo(const FloatPoint& targetPoint, bool, PathCoordinateMode mode)
{
    emitCommand1Arg(m_stringBuilder, (mode == AbsoluteCoordinates) ? 'M' : 'm', targetPoint);
}

void SVGPathStringBuilder::lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    emitCommand1Arg(m_stringBuilder, (mode == AbsoluteCoordinates) ? 'L' : 'l', targetPoint);
}

void SVGPathStringBuilder::lineToHorizontal(float x, PathCoordinateMode mode)
{
    emitCommand1Arg(m_stringBuilder, (mode == AbsoluteCoordinates) ? 'H' : 'h', x);
}

void SVGPathStringBuilder::lineToVertical(float y, PathCoordinateMode mode)
{
    emitCommand1Arg(m_stringBuilder, (mode == AbsoluteCoordinates) ? 'V' : 'v', y);
}

void SVGPathStringBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    m_stringBuilder.append((mode == AbsoluteCoordinates) ? 'C' : 'c');
    appendPoint(m_stringBuilder, point1);
    appendPoint(m_stringBuilder, point2);
    appendPoint(m_stringBuilder, targetPoint);
    m_stringBuilder.append(' ');
}

void SVGPathStringBuilder::curveToCubicSmooth(const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    emitCommand2Arg(m_stringBuilder, (mode == AbsoluteCoordinates) ? 'S' : 's', point2, targetPoint);
}

void SVGPathStringBuilder::curveToQuadratic(const FloatPoint& point1, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    emitCommand2Arg(m_stringBuilder, (mode == AbsoluteCoordinates) ? 'Q' : 'q', point1, targetPoint);
}

void SVGPathStringBuilder::curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    emitCommand1Arg(m_stringBuilder, (mode == AbsoluteCoordinates) ? 'T' : 't', targetPoint);
}

void SVGPathStringBuilder::arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    m_stringBuilder.append((mode == AbsoluteCoordinates) ? 'A' : 'a');
    appendFloat(m_stringBuilder, r1);
    appendFloat(m_stringBuilder, r2);
    appendFloat(m_stringBuilder, angle);
    appendBool(m_stringBuilder, largeArcFlag);
    appendBool(m_stringBuilder, sweepFlag);
    appendPoint(m_stringBuilder, targetPoint);
    m_stringBuilder.append(' ');
}

void SVGPathStringBuilder::closePath()
{
    m_stringBuilder.append("Z ");
}

} // namespace WebCore
