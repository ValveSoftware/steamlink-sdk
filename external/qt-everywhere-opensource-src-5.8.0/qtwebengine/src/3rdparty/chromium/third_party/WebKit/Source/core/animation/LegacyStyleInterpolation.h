// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LegacyStyleInterpolation_h
#define LegacyStyleInterpolation_h

#include "core/animation/StyleInterpolation.h"
#include "core/css/resolver/AnimatedStyleBuilder.h"
#include <memory>

namespace blink {

class LegacyStyleInterpolation : public StyleInterpolation {
public:
    static PassRefPtr<LegacyStyleInterpolation> create(PassRefPtr<AnimatableValue> start, PassRefPtr<AnimatableValue> end, CSSPropertyID id)
    {
        return adoptRef(new LegacyStyleInterpolation(InterpolableAnimatableValue::create(start), InterpolableAnimatableValue::create(end), id));
    }

    void apply(StyleResolverState& state) const override
    {
        AnimatedStyleBuilder::applyProperty(m_id, state, currentValue().get());
    }

    bool isLegacyStyleInterpolation() const final { return true; }
    PassRefPtr<AnimatableValue> currentValue() const
    {
        return toInterpolableAnimatableValue(m_cachedValue.get())->value();
    }

private:
    LegacyStyleInterpolation(std::unique_ptr<InterpolableValue> start, std::unique_ptr<InterpolableValue> end, CSSPropertyID id)
        : StyleInterpolation(std::move(start), std::move(end), id)
    {
    }
};

DEFINE_TYPE_CASTS(LegacyStyleInterpolation, Interpolation, value, value->isLegacyStyleInterpolation(), value.isLegacyStyleInterpolation());

} // namespace blink

#endif
