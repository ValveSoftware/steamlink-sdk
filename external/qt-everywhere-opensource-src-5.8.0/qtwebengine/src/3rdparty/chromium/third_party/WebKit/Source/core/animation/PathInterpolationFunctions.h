// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PathInterpolationFunctions_h
#define PathInterpolationFunctions_h

#include "core/animation/InterpolationType.h"
#include "core/svg/SVGPathByteStream.h"
#include <memory>

namespace blink {

class StylePath;

class PathInterpolationFunctions {
public:
    static std::unique_ptr<SVGPathByteStream> appliedValue(const InterpolableValue&, const NonInterpolableValue*);

    static void composite(UnderlyingValueOwner&, double underlyingFraction, const InterpolationType&, const InterpolationValue&);

    static InterpolationValue convertValue(const SVGPathByteStream&);

    static InterpolationValue convertValue(const StylePath*);

    static InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, InterpolationType::ConversionCheckers&);

    static PairwiseInterpolationValue maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end);
};

} // namespace blink

#endif // PathInterpolationFunctions_h
