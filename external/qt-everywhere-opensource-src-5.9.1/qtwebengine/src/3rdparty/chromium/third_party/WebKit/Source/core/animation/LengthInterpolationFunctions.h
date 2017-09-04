// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LengthInterpolationFunctions_h
#define LengthInterpolationFunctions_h

#include "core/animation/InterpolationValue.h"
#include "core/animation/PairwiseInterpolationValue.h"
#include "platform/Length.h"
#include <memory>

namespace blink {

class CSSToLengthConversionData;

class LengthInterpolationFunctions {
  STATIC_ONLY(LengthInterpolationFunctions);

 public:
  static std::unique_ptr<InterpolableValue> createInterpolablePixels(
      double pixels);
  static InterpolationValue createInterpolablePercent(double percent);
  static std::unique_ptr<InterpolableList> createNeutralInterpolableValue();

  static InterpolationValue maybeConvertCSSValue(const CSSValue&);
  static InterpolationValue maybeConvertLength(const Length&, float zoom);
  static PairwiseInterpolationValue mergeSingles(InterpolationValue&& start,
                                                 InterpolationValue&& end);
  static bool nonInterpolableValuesAreCompatible(const NonInterpolableValue*,
                                                 const NonInterpolableValue*);
  static void composite(std::unique_ptr<InterpolableValue>&,
                        RefPtr<NonInterpolableValue>&,
                        double underlyingFraction,
                        const InterpolableValue&,
                        const NonInterpolableValue*);
  static Length createLength(const InterpolableValue&,
                             const NonInterpolableValue*,
                             const CSSToLengthConversionData&,
                             ValueRange);

  static void subtractFromOneHundredPercent(InterpolationValue& result);
};

}  // namespace blink

#endif  // LengthInterpolationFunctions_h
