// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSShadowListInterpolationType_h
#define CSSShadowListInterpolationType_h

#include "core/animation/CSSInterpolationType.h"

namespace blink {

class ShadowList;

class CSSShadowListInterpolationType : public CSSInterpolationType {
public:
    CSSShadowListInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void composite(UnderlyingValueOwner&, double underlyingFraction, const InterpolationValue&, double interpolationFraction) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

private:
    InterpolationValue convertShadowList(const ShadowList*, double zoom) const;
    InterpolationValue createNeutralValue() const;

    InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;
    PairwiseInterpolationValue maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const final;
};

} // namespace blink

#endif // CSSShadowListInterpolationType_h
