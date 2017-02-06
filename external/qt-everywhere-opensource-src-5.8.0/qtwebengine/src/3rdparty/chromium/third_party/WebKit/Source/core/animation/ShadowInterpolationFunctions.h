// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ShadowInterpolationFunctions_h
#define ShadowInterpolationFunctions_h

#include "core/animation/InterpolationValue.h"
#include "core/animation/PairwiseInterpolationValue.h"
#include <memory>

namespace blink {

class ShadowData;
class CSSValue;
class StyleResolverState;

class ShadowInterpolationFunctions {
public:
    static InterpolationValue convertShadowData(const ShadowData&, double zoom);
    static InterpolationValue maybeConvertCSSValue(const CSSValue&);
    static std::unique_ptr<InterpolableValue> createNeutralInterpolableValue();
    static bool nonInterpolableValuesAreCompatible(const NonInterpolableValue*, const NonInterpolableValue*);
    static PairwiseInterpolationValue maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end);
    static void composite(std::unique_ptr<InterpolableValue>&, RefPtr<NonInterpolableValue>&, double underlyingFraction, const InterpolableValue&, const NonInterpolableValue*);
    static ShadowData createShadowData(const InterpolableValue&, const NonInterpolableValue*, const StyleResolverState&);
};

} // namespace blink

#endif // ShadowInterpolationFunctions_h
