// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintFilterEffect_h
#define PaintFilterEffect_h

#include "platform/graphics/filters/FilterEffect.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace blink {

class PLATFORM_EXPORT PaintFilterEffect : public FilterEffect {
public:
    static PaintFilterEffect* create(Filter*, const SkPaint&);
    ~PaintFilterEffect() override;

    FilterEffectType getFilterEffectType() const override { return FilterEffectTypeSourceInput; }

    TextStream& externalRepresentation(TextStream&, int indention) const override;
    sk_sp<SkImageFilter> createImageFilter() override;

private:
    PaintFilterEffect(Filter*, const SkPaint&);

    SkPaint m_paint;
};

} // namespace blink

#endif // PaintFilterEffect_h
