// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSValueInterpolationType_h
#define CSSValueInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

// Never supports pairwise conversion while always supporting single conversion.
// A catch all for default for CSSValues.
class CSSValueInterpolationType : public CSSInterpolationType {
public:
    CSSValueInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    PairwiseInterpolationValue maybeConvertPairwise(const PropertySpecificKeyframe& startKeyframe, const PropertySpecificKeyframe& endKeyframe, const InterpolationEnvironment&, const InterpolationValue& underlying, ConversionCheckers&) const final
    {
        return nullptr;
    }

    InterpolationValue maybeConvertSingle(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const InterpolationValue& underlying, ConversionCheckers&) const final;

    InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final
    {
        return nullptr;
    }

    // As we override CSSInterpolationType::maybeConvertSingle, these are never called.
    InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const final { NOTREACHED(); return nullptr; }
    InterpolationValue maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const final { NOTREACHED(); return nullptr; }
    InterpolationValue maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final { NOTREACHED(); return nullptr; }
    InterpolationValue maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const final { NOTREACHED(); return nullptr; }

    void composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const final
    {
        underlyingValueOwner.set(*this, value);
    }

    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;
};

} // namespace blink

#endif // CSSValueInterpolationType_h
