/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/svg/SVGPathBuilder.h"

#include "platform/graphics/Path.h"

namespace blink {

FloatPoint SVGPathBuilder::smoothControl(bool isCompatibleSegment) const
{
    // The control point is assumed to be the reflection of the control point on
    // the previous command relative to the current point. If there is no previous
    // command or if the previous command was not a [quad/cubic], assume the control
    // point is coincident with the current point.
    // [https://www.w3.org/TR/SVG/paths.html#PathDataCubicBezierCommands]
    // [https://www.w3.org/TR/SVG/paths.html#PathDataQuadraticBezierCommands]
    FloatPoint controlPoint = m_currentPoint;
    if (isCompatibleSegment)
        controlPoint += m_currentPoint - m_lastControlPoint;

    return controlPoint;
}

void SVGPathBuilder::emitClose()
{
    m_path.closeSubpath();

    // At the end of the [closepath] command, the new current
    // point is set to the initial point of the current subpath.
    // [https://www.w3.org/TR/SVG/paths.html#PathDataClosePathCommand]
    m_currentPoint = m_subpathPoint;
}

void SVGPathBuilder::emitMoveTo(const FloatPoint& p)
{
    m_path.moveTo(p);

    m_subpathPoint = p;
    m_currentPoint = p;
}

void SVGPathBuilder::emitLineTo(const FloatPoint& p)
{
    m_path.addLineTo(p);
    m_currentPoint = p;
}

void SVGPathBuilder::emitQuadTo(const FloatPoint& c0, const FloatPoint& p)
{
    m_path.addQuadCurveTo(c0, p);
    m_lastControlPoint = c0;
    m_currentPoint = p;
}

void SVGPathBuilder::emitSmoothQuadTo(const FloatPoint& p)
{
    bool lastWasQuadratic = m_lastCommand == PathSegCurveToQuadraticAbs
        || m_lastCommand == PathSegCurveToQuadraticRel
        || m_lastCommand == PathSegCurveToQuadraticSmoothAbs
        || m_lastCommand == PathSegCurveToQuadraticSmoothRel;

    emitQuadTo(smoothControl(lastWasQuadratic), p);
}

void SVGPathBuilder::emitCubicTo(const FloatPoint& c0, const FloatPoint& c1, const FloatPoint& p)
{
    m_path.addBezierCurveTo(c0, c1, p);
    m_lastControlPoint = c1;
    m_currentPoint = p;
}

void SVGPathBuilder::emitSmoothCubicTo(const FloatPoint& c1, const FloatPoint& p)
{
    bool lastWasCubic = m_lastCommand == PathSegCurveToCubicAbs
        || m_lastCommand == PathSegCurveToCubicRel
        || m_lastCommand == PathSegCurveToCubicSmoothAbs
        || m_lastCommand == PathSegCurveToCubicSmoothRel;

    emitCubicTo(smoothControl(lastWasCubic), c1, p);
}

void SVGPathBuilder::emitArcTo(const FloatPoint& p, const FloatSize& r, float rotate,
    bool largeArc, bool sweep)
{
    m_path.addArcTo(p, r, rotate, largeArc, sweep);
    m_currentPoint = p;
}

void SVGPathBuilder::emitSegment(const PathSegmentData& segment)
{
    switch (segment.command) {
    case PathSegClosePath:
        emitClose();
        break;
    case PathSegMoveToAbs:
        emitMoveTo(
            segment.targetPoint);
        break;
    case PathSegMoveToRel:
        emitMoveTo(
            m_currentPoint + segment.targetPoint);
        break;
    case PathSegLineToAbs:
        emitLineTo(
            segment.targetPoint);
        break;
    case PathSegLineToRel:
        emitLineTo(
            m_currentPoint + segment.targetPoint);
        break;
    case PathSegLineToHorizontalAbs:
        emitLineTo(
            FloatPoint(segment.targetPoint.x(), m_currentPoint.y()));
        break;
    case PathSegLineToHorizontalRel:
        emitLineTo(
            m_currentPoint + FloatSize(segment.targetPoint.x(), 0));
        break;
    case PathSegLineToVerticalAbs:
        emitLineTo(
            FloatPoint(m_currentPoint.x(), segment.targetPoint.y()));
        break;
    case PathSegLineToVerticalRel:
        emitLineTo(
            m_currentPoint + FloatSize(0, segment.targetPoint.y()));
        break;
    case PathSegCurveToQuadraticAbs:
        emitQuadTo(
            segment.point1,
            segment.targetPoint);
        break;
    case PathSegCurveToQuadraticRel:
        emitQuadTo(
            m_currentPoint + segment.point1,
            m_currentPoint + segment.targetPoint);
        break;
    case PathSegCurveToQuadraticSmoothAbs:
        emitSmoothQuadTo(
            segment.targetPoint);
        break;
    case PathSegCurveToQuadraticSmoothRel:
        emitSmoothQuadTo(
            m_currentPoint + segment.targetPoint);
        break;
    case PathSegCurveToCubicAbs:
        emitCubicTo(
            segment.point1,
            segment.point2,
            segment.targetPoint);
        break;
    case PathSegCurveToCubicRel:
        emitCubicTo(
            m_currentPoint + segment.point1,
            m_currentPoint + segment.point2,
            m_currentPoint + segment.targetPoint);
        break;
    case PathSegCurveToCubicSmoothAbs:
        emitSmoothCubicTo(
            segment.point2,
            segment.targetPoint);
        break;
    case PathSegCurveToCubicSmoothRel:
        emitSmoothCubicTo(
            m_currentPoint + segment.point2,
            m_currentPoint + segment.targetPoint);
        break;
    case PathSegArcAbs:
        emitArcTo(
            segment.targetPoint,
            toFloatSize(segment.arcRadii()),
            segment.arcAngle(),
            segment.largeArcFlag(),
            segment.sweepFlag());
        break;
    case PathSegArcRel:
        emitArcTo(
            m_currentPoint + segment.targetPoint,
            toFloatSize(segment.arcRadii()),
            segment.arcAngle(),
            segment.largeArcFlag(),
            segment.sweepFlag());
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_lastCommand = segment.command;
}

} // namespace blink
