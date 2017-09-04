// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BasicShapeInterpolationFunctions_h
#define BasicShapeInterpolationFunctions_h

#include "core/animation/InterpolationValue.h"
#include <memory>

namespace blink {

class BasicShape;
class CSSValue;
class CSSToLengthConversionData;

namespace BasicShapeInterpolationFunctions {

InterpolationValue maybeConvertCSSValue(const CSSValue&);
InterpolationValue maybeConvertBasicShape(const BasicShape*, double zoom);
std::unique_ptr<InterpolableValue> createNeutralValue(
    const NonInterpolableValue&);
bool shapesAreCompatible(const NonInterpolableValue&,
                         const NonInterpolableValue&);
PassRefPtr<BasicShape> createBasicShape(const InterpolableValue&,
                                        const NonInterpolableValue&,
                                        const CSSToLengthConversionData&);

}  // namespace BasicShapeInterpolationFunctions

}  // namespace blink

#endif  // BasicShapeInterpolationFunctions_h
