// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGNumberOptionalNumberInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/svg/SVGNumberOptionalNumber.h"
#include <memory>

namespace blink {

InterpolationValue SVGNumberOptionalNumberInterpolationType::maybeConvertNeutral(const InterpolationValue&, ConversionCheckers&) const
{
    std::unique_ptr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, InterpolableNumber::create(0));
    result->set(1, InterpolableNumber::create(0));
    return InterpolationValue(std::move(result));
}

InterpolationValue SVGNumberOptionalNumberInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedNumberOptionalNumber)
        return nullptr;

    const SVGNumberOptionalNumber& numberOptionalNumber = toSVGNumberOptionalNumber(svgValue);
    std::unique_ptr<InterpolableList> result = InterpolableList::create(2);
    result->set(0, InterpolableNumber::create(numberOptionalNumber.firstNumber()->value()));
    result->set(1, InterpolableNumber::create(numberOptionalNumber.secondNumber()->value()));
    return InterpolationValue(std::move(result));
}

SVGPropertyBase* SVGNumberOptionalNumberInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*) const
{
    const InterpolableList& list = toInterpolableList(interpolableValue);
    return SVGNumberOptionalNumber::create(
        SVGNumber::create(toInterpolableNumber(list.get(0))->value()),
        SVGNumber::create(toInterpolableNumber(list.get(1))->value()));
}

} // namespace blink
