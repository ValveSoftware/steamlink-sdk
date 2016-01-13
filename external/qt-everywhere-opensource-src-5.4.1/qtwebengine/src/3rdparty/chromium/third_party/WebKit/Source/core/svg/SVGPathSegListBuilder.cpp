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

#include "config.h"

#include "core/svg/SVGPathSegListBuilder.h"

#include "core/dom/ExceptionCode.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg/SVGPathSegArcAbs.h"
#include "core/svg/SVGPathSegArcRel.h"
#include "core/svg/SVGPathSegClosePath.h"
#include "core/svg/SVGPathSegCurvetoCubicAbs.h"
#include "core/svg/SVGPathSegCurvetoCubicRel.h"
#include "core/svg/SVGPathSegCurvetoCubicSmoothAbs.h"
#include "core/svg/SVGPathSegCurvetoCubicSmoothRel.h"
#include "core/svg/SVGPathSegCurvetoQuadraticAbs.h"
#include "core/svg/SVGPathSegCurvetoQuadraticRel.h"
#include "core/svg/SVGPathSegCurvetoQuadraticSmoothAbs.h"
#include "core/svg/SVGPathSegCurvetoQuadraticSmoothRel.h"
#include "core/svg/SVGPathSegLinetoAbs.h"
#include "core/svg/SVGPathSegLinetoHorizontalAbs.h"
#include "core/svg/SVGPathSegLinetoHorizontalRel.h"
#include "core/svg/SVGPathSegLinetoRel.h"
#include "core/svg/SVGPathSegLinetoVerticalAbs.h"
#include "core/svg/SVGPathSegLinetoVerticalRel.h"
#include "core/svg/SVGPathSegMovetoAbs.h"
#include "core/svg/SVGPathSegMovetoRel.h"

namespace WebCore {

SVGPathSegListBuilder::SVGPathSegListBuilder()
    : m_pathElement(0)
    , m_pathSegList(nullptr)
    , m_pathSegRole(PathSegUndefinedRole)
{
}

void SVGPathSegListBuilder::moveTo(const FloatPoint& targetPoint, bool, PathCoordinateMode mode)
{
    ASSERT(m_pathElement);
    ASSERT(m_pathSegList);
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegMovetoAbs::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y()));
    else
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegMovetoRel::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y()));
}

void SVGPathSegListBuilder::lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    ASSERT(m_pathElement);
    ASSERT(m_pathSegList);
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegLinetoAbs::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y()));
    else
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegLinetoRel::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y()));
}

void SVGPathSegListBuilder::lineToHorizontal(float x, PathCoordinateMode mode)
{
    ASSERT(m_pathElement);
    ASSERT(m_pathSegList);
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegLinetoHorizontalAbs::create(m_pathElement, m_pathSegRole, x));
    else
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegLinetoHorizontalRel::create(m_pathElement, m_pathSegRole, x));
}

void SVGPathSegListBuilder::lineToVertical(float y, PathCoordinateMode mode)
{
    ASSERT(m_pathElement);
    ASSERT(m_pathSegList);
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegLinetoVerticalAbs::create(m_pathElement, m_pathSegRole, y));
    else
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegLinetoVerticalRel::create(m_pathElement, m_pathSegRole, y));
}

void SVGPathSegListBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    ASSERT(m_pathElement);
    ASSERT(m_pathSegList);
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegCurvetoCubicAbs::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y(), point1.x(), point1.y(), point2.x(), point2.y()));
    else
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegCurvetoCubicRel::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y(), point1.x(), point1.y(), point2.x(), point2.y()));
}

void SVGPathSegListBuilder::curveToCubicSmooth(const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    ASSERT(m_pathElement);
    ASSERT(m_pathSegList);
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegCurvetoCubicSmoothAbs::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y(), point2.x(), point2.y()));
    else
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegCurvetoCubicSmoothRel::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y(), point2.x(), point2.y()));
}

void SVGPathSegListBuilder::curveToQuadratic(const FloatPoint& point1, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    ASSERT(m_pathElement);
    ASSERT(m_pathSegList);
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegCurvetoQuadraticAbs::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y(), point1.x(), point1.y()));
    else
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegCurvetoQuadraticRel::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y(), point1.x(), point1.y()));
}

void SVGPathSegListBuilder::curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    ASSERT(m_pathElement);
    ASSERT(m_pathSegList);
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegCurvetoQuadraticSmoothAbs::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y()));
    else
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegCurvetoQuadraticSmoothRel::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y()));
}

void SVGPathSegListBuilder::arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    ASSERT(m_pathElement);
    ASSERT(m_pathSegList);
    if (mode == AbsoluteCoordinates)
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegArcAbs::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y(), r1, r2, angle, largeArcFlag, sweepFlag));
    else
        m_pathSegList->appendWithoutByteStreamSync(SVGPathSegArcRel::create(m_pathElement, m_pathSegRole, targetPoint.x(), targetPoint.y(), r1, r2, angle, largeArcFlag, sweepFlag));
}

void SVGPathSegListBuilder::closePath()
{
    ASSERT(m_pathElement);
    ASSERT(m_pathSegList);
    m_pathSegList->appendWithoutByteStreamSync(SVGPathSegClosePath::create(m_pathElement, m_pathSegRole));
}

}
