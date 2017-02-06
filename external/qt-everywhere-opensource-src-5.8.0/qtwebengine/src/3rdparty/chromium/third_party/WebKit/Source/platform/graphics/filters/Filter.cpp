/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "platform/graphics/filters/Filter.h"

#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/SourceGraphic.h"

namespace blink {

Filter::Filter(const FloatRect& referenceBox, const FloatRect& filterRegion, float scale, UnitScaling unitScaling)
    : m_referenceBox(referenceBox)
    , m_filterRegion(filterRegion)
    , m_scale(scale)
    , m_unitScaling(unitScaling)
    , m_sourceGraphic(SourceGraphic::create(this))
{
}

Filter* Filter::create(const FloatRect& referenceBox, const FloatRect& filterRegion, float scale, UnitScaling unitScaling)
{
    return new Filter(referenceBox, filterRegion, scale, unitScaling);
}

Filter* Filter::create(float scale)
{
    return new Filter(FloatRect(), FloatRect(), scale, UserSpace);
}

DEFINE_TRACE(Filter)
{
    visitor->trace(m_sourceGraphic);
    visitor->trace(m_lastEffect);
}

FloatRect Filter::mapLocalRectToAbsoluteRect(const FloatRect& rect) const
{
    FloatRect result(rect);
    result.scale(m_scale);
    return result;
}

FloatRect Filter::mapAbsoluteRectToLocalRect(const FloatRect& rect) const
{
    FloatRect result(rect);
    result.scale(1.0f / m_scale);
    return result;
}

float Filter::applyHorizontalScale(float value) const
{
    if (m_unitScaling == BoundingBox)
        value *= referenceBox().width();
    return m_scale * value;
}

float Filter::applyVerticalScale(float value) const
{
    if (m_unitScaling == BoundingBox)
        value *= referenceBox().height();
    return m_scale * value;
}

FloatPoint3D Filter::resolve3dPoint(const FloatPoint3D& point) const
{
    if (m_unitScaling != BoundingBox)
        return point;
    return FloatPoint3D(point.x() * referenceBox().width() + referenceBox().x(),
        point.y() * referenceBox().height() + referenceBox().y(),
        point.z() * sqrtf(referenceBox().size().diagonalLengthSquared() / 2));
}

void Filter::setLastEffect(FilterEffect* effect)
{
    m_lastEffect = effect;
}

} // namespace blink
