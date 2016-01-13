/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef SVGFilter_h
#define SVGFilter_h

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class SVGFilter FINAL : public Filter {
public:
    static PassRefPtr<SVGFilter> create(const AffineTransform&, const IntRect&, const FloatRect&, const FloatRect&, bool);

    virtual float applyHorizontalScale(float value) const OVERRIDE;
    virtual float applyVerticalScale(float value) const OVERRIDE;
    virtual FloatPoint3D resolve3dPoint(const FloatPoint3D&) const OVERRIDE;

    virtual IntRect sourceImageRect() const OVERRIDE { return m_absoluteSourceDrawingRegion; }
    FloatRect targetBoundingBox() const { return m_targetBoundingBox; }

private:
    SVGFilter(const AffineTransform& absoluteTransform, const IntRect& absoluteSourceDrawingRegion, const FloatRect& targetBoundingBox, const FloatRect& filterRegion, bool effectBBoxMode);

    IntRect m_absoluteSourceDrawingRegion;
    FloatRect m_targetBoundingBox;
    bool m_effectBBoxMode;
};

} // namespace WebCore

#endif // SVGFilter_h
