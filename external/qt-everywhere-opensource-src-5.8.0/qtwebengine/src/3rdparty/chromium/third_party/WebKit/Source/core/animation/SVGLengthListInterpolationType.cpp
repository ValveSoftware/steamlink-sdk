// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGLengthListInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/SVGLengthInterpolationType.h"
#include "core/animation/UnderlyingLengthChecker.h"
#include "core/svg/SVGLengthList.h"
#include <memory>

namespace blink {

InterpolationValue SVGLengthListInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    size_t underlyingLength = UnderlyingLengthChecker::getUnderlyingLength(underlying);
    conversionCheckers.append(UnderlyingLengthChecker::create(underlyingLength));

    if (underlyingLength == 0)
        return nullptr;

    std::unique_ptr<InterpolableList> result = InterpolableList::create(underlyingLength);
    for (size_t i = 0; i < underlyingLength; i++)
        result->set(i, SVGLengthInterpolationType::neutralInterpolableValue());
    return InterpolationValue(std::move(result));
}

InterpolationValue SVGLengthListInterpolationType::maybeConvertSVGValue(const SVGPropertyBase& svgValue) const
{
    if (svgValue.type() != AnimatedLengthList)
        return nullptr;

    const SVGLengthList& lengthList = toSVGLengthList(svgValue);
    std::unique_ptr<InterpolableList> result = InterpolableList::create(lengthList.length());
    for (size_t i = 0; i < lengthList.length(); i++) {
        InterpolationValue component = SVGLengthInterpolationType::convertSVGLength(*lengthList.at(i));
        result->set(i, std::move(component.interpolableValue));
    }
    return InterpolationValue(std::move(result));
}

PairwiseInterpolationValue SVGLengthListInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    size_t startLength = toInterpolableList(*start.interpolableValue).length();
    size_t endLength = toInterpolableList(*end.interpolableValue).length();
    if (startLength != endLength)
        return nullptr;
    return InterpolationType::maybeMergeSingles(std::move(start), std::move(end));
}

void SVGLengthListInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    size_t startLength = toInterpolableList(*underlyingValueOwner.value().interpolableValue).length();
    size_t endLength = toInterpolableList(*value.interpolableValue).length();

    if (startLength == endLength)
        InterpolationType::composite(underlyingValueOwner, underlyingFraction, value, interpolationFraction);
    else
        underlyingValueOwner.set(*this, value);
}

SVGPropertyBase* SVGLengthListInterpolationType::appliedSVGValue(const InterpolableValue& interpolableValue, const NonInterpolableValue*) const
{
    NOTREACHED();
    // This function is no longer called, because apply has been overridden.
    return nullptr;
}

void SVGLengthListInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    SVGElement& element = environment.svgElement();
    SVGLengthContext lengthContext(&element);

    SVGLengthList* result = SVGLengthList::create(m_unitMode);
    const InterpolableList& list = toInterpolableList(interpolableValue);
    for (size_t i = 0; i < list.length(); i++) {
        result->append(SVGLengthInterpolationType::resolveInterpolableSVGLength(*list.get(i), lengthContext, m_unitMode, m_negativeValuesForbidden));
    }

    element.setWebAnimatedAttribute(attribute(), result);
}

} // namespace blink
