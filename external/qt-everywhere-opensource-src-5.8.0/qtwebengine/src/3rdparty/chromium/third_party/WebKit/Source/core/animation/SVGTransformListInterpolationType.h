// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGTransformListInterpolationType_h
#define SVGTransformListInterpolationType_h

#include "core/animation/SVGInterpolationType.h"
#include "core/svg/SVGTransform.h"

namespace blink {

class SVGTransformListInterpolationType : public SVGInterpolationType {
public:
    SVGTransformListInterpolationType(const QualifiedName& attribute)
        : SVGInterpolationType(attribute)
    { }

private:
    InterpolationValue maybeConvertNeutral(const InterpolationValue&, ConversionCheckers&) const final;
    InterpolationValue maybeConvertSVGValue(const SVGPropertyBase& svgValue) const final;
    InterpolationValue maybeConvertSingle(const PropertySpecificKeyframe&, const InterpolationEnvironment&, const InterpolationValue& underlying, ConversionCheckers&) const final;
    SVGPropertyBase* appliedSVGValue(const InterpolableValue&, const NonInterpolableValue*) const final;

    PairwiseInterpolationValue maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const final;
    void composite(UnderlyingValueOwner&, double underlyingFraction, const InterpolationValue&, double interpolationFraction) const final;
};

} // namespace blink

#endif // SVGTransformListInterpolationType_h
