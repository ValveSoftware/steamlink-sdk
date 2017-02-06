/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
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

#include "platform/graphics/filters/FEMorphology.h"

#include "SkMorphologyImageFilter.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/text/TextStream.h"

namespace blink {

FEMorphology::FEMorphology(Filter* filter, MorphologyOperatorType type, float radiusX, float radiusY)
    : FilterEffect(filter)
    , m_type(type)
    , m_radiusX(std::max(0.0f, radiusX))
    , m_radiusY(std::max(0.0f, radiusY))
{
}

FEMorphology* FEMorphology::create(Filter* filter, MorphologyOperatorType type, float radiusX, float radiusY)
{
    return new FEMorphology(filter, type, radiusX, radiusY);
}

MorphologyOperatorType FEMorphology::morphologyOperator() const
{
    return m_type;
}

bool FEMorphology::setMorphologyOperator(MorphologyOperatorType type)
{
    if (m_type == type)
        return false;
    m_type = type;
    return true;
}

float FEMorphology::radiusX() const
{
    return m_radiusX;
}

bool FEMorphology::setRadiusX(float radiusX)
{
    radiusX = std::max(0.0f, radiusX);
    if (m_radiusX == radiusX)
        return false;
    m_radiusX = radiusX;
    return true;
}

float FEMorphology::radiusY() const
{
    return m_radiusY;
}

bool FEMorphology::setRadiusY(float radiusY)
{
    radiusY = std::max(0.0f, radiusY);
    if (m_radiusY == radiusY)
        return false;
    m_radiusY = radiusY;
    return true;
}

FloatRect FEMorphology::mapRect(const FloatRect& rect, bool) const
{
    FloatRect result = rect;
    result.inflateX(getFilter()->applyHorizontalScale(m_radiusX));
    result.inflateY(getFilter()->applyVerticalScale(m_radiusY));
    return result;
}

sk_sp<SkImageFilter> FEMorphology::createImageFilter()
{
    sk_sp<SkImageFilter> input(SkiaImageFilterBuilder::build(inputEffect(0), operatingColorSpace()));
    SkScalar radiusX = SkFloatToScalar(getFilter()->applyHorizontalScale(m_radiusX));
    SkScalar radiusY = SkFloatToScalar(getFilter()->applyVerticalScale(m_radiusY));
    SkImageFilter::CropRect rect = getCropRect();
    if (m_type == FEMORPHOLOGY_OPERATOR_DILATE)
        return SkDilateImageFilter::Make(radiusX, radiusY, std::move(input), &rect);
    return SkErodeImageFilter::Make(radiusX, radiusY, std::move(input), &rect);
}

static TextStream& operator<<(TextStream& ts, const MorphologyOperatorType& type)
{
    switch (type) {
    case FEMORPHOLOGY_OPERATOR_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case FEMORPHOLOGY_OPERATOR_ERODE:
        ts << "ERODE";
        break;
    case FEMORPHOLOGY_OPERATOR_DILATE:
        ts << "DILATE";
        break;
    }
    return ts;
}

TextStream& FEMorphology::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feMorphology";
    FilterEffect::externalRepresentation(ts);
    ts << " operator=\"" << morphologyOperator() << "\" "
        << "radius=\"" << radiusX() << ", " << radiusY() << "\"]\n";
    inputEffect(0)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace blink
