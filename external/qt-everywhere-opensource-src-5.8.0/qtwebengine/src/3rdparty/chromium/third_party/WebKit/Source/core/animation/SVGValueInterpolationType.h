// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGValueInterpolationType_h
#define SVGValueInterpolationType_h

#include "core/animation/SVGInterpolationType.h"

namespace blink {

// Never supports pairwise conversion while always supporting single conversion.
// A catch all default for SVGValues.
class SVGValueInterpolationType : public SVGInterpolationType {
public:
    SVGValueInterpolationType(const QualifiedName& attribute)
        : SVGInterpolationType(attribute)
    { }

private:
    PairwiseInterpolationValue maybeConvertPairwise(const PropertySpecificKeyframe& startKeyframe, const PropertySpecificKeyframe& endKeyframe, const InterpolationEnvironment&, const InterpolationValue& underlying, ConversionCheckers&) const final
    {
        return nullptr;
    }

    InterpolationValue maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const final
    {
        return nullptr;
    }

    InterpolationValue maybeConvertSVGValue(const SVGPropertyBase&) const final;

    InterpolationValue maybeConvertUnderlyingValue(const InterpolationEnvironment&) const final
    {
        return nullptr;
    }

    void composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const final
    {
        underlyingValueOwner.set(*this, value);
    }

    SVGPropertyBase* appliedSVGValue(const InterpolableValue&, const NonInterpolableValue*) const final;
};

} // namespace blink

#endif // SVGValueInterpolationType_h
