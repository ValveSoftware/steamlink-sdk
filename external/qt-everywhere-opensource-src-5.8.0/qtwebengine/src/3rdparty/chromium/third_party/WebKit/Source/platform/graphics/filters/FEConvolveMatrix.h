/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Zoltan Herczeg <zherczeg@webkit.org>
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

#ifndef FEConvolveMatrix_h
#define FEConvolveMatrix_h

#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "wtf/Vector.h"

namespace blink {

enum EdgeModeType {
    EDGEMODE_UNKNOWN   = 0,
    EDGEMODE_DUPLICATE = 1,
    EDGEMODE_WRAP      = 2,
    EDGEMODE_NONE      = 3
};

class PLATFORM_EXPORT FEConvolveMatrix final : public FilterEffect {
public:
    static FEConvolveMatrix* create(Filter*, const IntSize&,
        float, float, const IntPoint&, EdgeModeType, bool, const Vector<float>&);

    bool setDivisor(float);
    bool setBias(float);
    bool setTargetOffset(const IntPoint&);
    bool setEdgeMode(EdgeModeType);
    bool setPreserveAlpha(bool);

    FloatRect mapPaintRect(const FloatRect&, bool forward = true) const final;

    TextStream& externalRepresentation(TextStream&, int indention) const override;

private:
    FEConvolveMatrix(Filter*, const IntSize&, float, float,
        const IntPoint&, EdgeModeType, bool, const Vector<float>&);

    sk_sp<SkImageFilter> createImageFilter() override;

    bool parametersValid() const;

    IntSize m_kernelSize;
    float m_divisor;
    float m_bias;
    IntPoint m_targetOffset;
    EdgeModeType m_edgeMode;
    bool m_preserveAlpha;
    Vector<float> m_kernelMatrix;
};

} // namespace blink

#endif // FEConvolveMatrix_h
