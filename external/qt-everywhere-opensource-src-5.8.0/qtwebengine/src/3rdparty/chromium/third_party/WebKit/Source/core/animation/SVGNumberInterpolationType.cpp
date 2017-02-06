// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGNumberInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/SVGNumber.h"
#include "core/svg/properties/SVGAnimatedProperty.h"

namespace blink {

InterpolationValue SVGNumberInterpolationType::maybeConvertNeutral(const InterpolationValue&, ConversionCheckers&) const
{
    return InterpolationValue(InterpolableNumber::create(0));
}

InterpolationValue SVGNumberInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedNumber)
        return nullptr;
    return InterpolationValue(InterpolableNumber::create(toSVGNumber(svgValue).value()));
}

SVGPropertyBase* SVGNumberInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*) const
{
    double value = toInterpolableNumber(interpolableValue).value();
    return SVGNumber::create(m_isNonNegative && value < 0 ? 0 : value);
}

} // namespace blink
