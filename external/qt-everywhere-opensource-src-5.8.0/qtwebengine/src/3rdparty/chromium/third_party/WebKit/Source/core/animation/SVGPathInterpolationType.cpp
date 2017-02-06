// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGPathInterpolationType.h"

#include "core/animation/PathInterpolationFunctions.h"

#include "core/svg/SVGPath.h"

namespace blink {

InterpolationValue SVGPathInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedPath)
        return nullptr;

    return PathInterpolationFunctions::convertValue(toSVGPath(svgValue).byteStream());
}

InterpolationValue SVGPathInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    return PathInterpolationFunctions::maybeConvertNeutral(underlying, conversionCheckers);
}

PairwiseInterpolationValue SVGPathInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    return PathInterpolationFunctions::maybeMergeSingles(std::move(start), std::move(end));
}

void SVGPathInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    PathInterpolationFunctions::composite(underlyingValueOwner, underlyingFraction, *this, value);
}

SVGPropertyBase* SVGPathInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue) const
{
    return SVGPath::create(CSSPathValue::create(PathInterpolationFunctions::appliedValue(interpolableValue, nonInterpolableValue)));
}

} // namespace blink
