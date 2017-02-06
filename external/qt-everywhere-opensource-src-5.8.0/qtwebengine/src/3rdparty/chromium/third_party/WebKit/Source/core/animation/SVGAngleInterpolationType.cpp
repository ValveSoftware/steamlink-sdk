// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGAngleInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/svg/SVGAngle.h"

namespace blink {

InterpolationValue SVGAngleInterpolationType::maybeConvertNeutral(const InterpolationValue&, ConversionCheckers&) const
{
    return InterpolationValue(InterpolableNumber::create(0));
}

InterpolationValue SVGAngleInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (toSVGAngle(svgValue).orientType()->enumValue() != SVGMarkerOrientAngle)
        return nullptr;
    return InterpolationValue(InterpolableNumber::create(toSVGAngle(svgValue).value()));
}

SVGPropertyBase* SVGAngleInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*) const
{
    double doubleValue = toInterpolableNumber(interpolableValue).value();
    SVGAngle* result = SVGAngle::create();
    result->newValueSpecifiedUnits(SVGAngle::SVG_ANGLETYPE_DEG, doubleValue);
    return result;
}

} // namespace blink
