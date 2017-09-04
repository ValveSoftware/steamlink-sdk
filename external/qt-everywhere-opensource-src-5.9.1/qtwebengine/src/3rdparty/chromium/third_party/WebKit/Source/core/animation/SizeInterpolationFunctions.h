// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SizeInterpolationFunctions_h
#define SizeInterpolationFunctions_h

#include "core/animation/InterpolationValue.h"
#include "core/animation/PairwiseInterpolationValue.h"
#include "core/style/FillLayer.h"
#include <memory>

namespace blink {

class CSSToLengthConversionData;
class CSSValue;

class SizeInterpolationFunctions {
  STATIC_ONLY(SizeInterpolationFunctions);

 public:
  static InterpolationValue convertFillSizeSide(const FillSize&,
                                                float zoom,
                                                bool convertWidth);
  static InterpolationValue maybeConvertCSSSizeSide(const CSSValue&,
                                                    bool convertWidth);
  static PairwiseInterpolationValue maybeMergeSingles(
      InterpolationValue&& start,
      InterpolationValue&& end);
  static InterpolationValue createNeutralValue(const NonInterpolableValue*);
  static bool nonInterpolableValuesAreCompatible(const NonInterpolableValue*,
                                                 const NonInterpolableValue*);
  static void composite(std::unique_ptr<InterpolableValue>&,
                        RefPtr<NonInterpolableValue>&,
                        double underlyingFraction,
                        const InterpolableValue&,
                        const NonInterpolableValue*);
  static FillSize createFillSize(
      const InterpolableValue& interpolableValueA,
      const NonInterpolableValue* nonInterpolableValueA,
      const InterpolableValue& interpolableValueB,
      const NonInterpolableValue* nonInterpolableValueB,
      const CSSToLengthConversionData&);
};

}  // namespace blink

#endif  // SizeInterpolationFunctions_h
