// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSImageListInterpolationType_h
#define CSSImageListInterpolationType_h

#include "core/animation/CSSInterpolationType.h"
#include "core/animation/ImageListPropertyFunctions.h"

namespace blink {

class CSSImageListInterpolationType : public CSSInterpolationType {
public:
    CSSImageListInterpolationType(CSSPropertyID property)
        : CSSInterpolationType(property)
    { }

    InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final;
    void composite(UnderlyingValueOwner&, double underlyingFraction, const InterpolationValue&, double interpolationFraction) const final;
    void apply(const InterpolableValue&, const NonInterpolableValue*, InterpolationEnvironment&) const final;

private:
    InterpolationValue maybeConvertStyleImageList(const StyleImageList&) const;

    InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertInherit(const StyleResolverState&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertValue(const CSSValue&, const StyleResolverState&, ConversionCheckers&) const final;

    PairwiseInterpolationValue maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const final;
};

} // namespace blink

#endif // CSSImageListInterpolationType_h
