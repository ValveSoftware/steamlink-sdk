/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/svg/graphics/filters/SVGFilter.h"

namespace WebCore {

SVGFilter::SVGFilter(const AffineTransform& absoluteTransform, const IntRect& absoluteSourceDrawingRegion, const FloatRect& targetBoundingBox, const FloatRect& filterRegion, bool effectBBoxMode)
    : Filter(absoluteTransform)
    , m_absoluteSourceDrawingRegion(absoluteSourceDrawingRegion)
    , m_targetBoundingBox(targetBoundingBox)
    , m_effectBBoxMode(effectBBoxMode)
{
    setFilterRegion(filterRegion);
}

float SVGFilter::applyHorizontalScale(float value) const
{
    if (m_effectBBoxMode)
        value *= m_targetBoundingBox.width();
    return Filter::applyHorizontalScale(value);
}

float SVGFilter::applyVerticalScale(float value) const
{
    if (m_effectBBoxMode)
        value *= m_targetBoundingBox.height();
    return Filter::applyVerticalScale(value);
}

FloatPoint3D SVGFilter::resolve3dPoint(const FloatPoint3D& point) const
{
    if (!m_effectBBoxMode)
        return point;
    return FloatPoint3D(point.x() * m_targetBoundingBox.width() + m_targetBoundingBox.x(),
        point.y() * m_targetBoundingBox.height() + m_targetBoundingBox.y(),
        point.z() * sqrtf(m_targetBoundingBox.size().diagonalLengthSquared() / 2));
}

PassRefPtr<SVGFilter> SVGFilter::create(const AffineTransform& absoluteTransform, const IntRect& absoluteSourceDrawingRegion, const FloatRect& targetBoundingBox, const FloatRect& filterRegion, bool effectBBoxMode)
{
    return adoptRef(new SVGFilter(absoluteTransform, absoluteSourceDrawingRegion, targetBoundingBox, filterRegion, effectBBoxMode));
}

} // namespace WebCore
