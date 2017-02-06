// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGIntegerInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/svg/SVGInteger.h"

namespace blink {

InterpolationValue SVGIntegerInterpolationType::maybeConvertNeutral(const InterpolationValue&, ConversionCheckers&) const
{
    return InterpolationValue(InterpolableNumber::create(0));
}

InterpolationValue SVGIntegerInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedInteger)
        return nullptr;
    return InterpolationValue(InterpolableNumber::create(toSVGInteger(svgValue).value()));
}

SVGPropertyBase* SVGIntegerInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*) const
{
    double value = toInterpolableNumber(interpolableValue).value();
    return SVGInteger::create(round(value));
}

} // namespace blink
