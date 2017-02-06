// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSNumberInterpolationType.h"

#include "core/animation/NumberPropertyFunctions.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class ParentNumberChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<ParentNumberChecker> create(CSSPropertyID property, double number)
    {
        return wrapUnique(new ParentNumberChecker(property, number));
    }

private:
    ParentNumberChecker(CSSPropertyID property, double number)
        : m_property(property)
        , m_number(number)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        double parentNumber;
        if (!NumberPropertyFunctions::getNumber(m_property, *environment.state().parentStyle(), parentNumber))
            return false;
        return parentNumber == m_number;
    }

    const CSSPropertyID m_property;
    const double m_number;
};

InterpolationValue CSSNumberInterpolationType::createNumberValue(double number) const
{
    return InterpolationValue(InterpolableNumber::create(number));
}

InterpolationValue CSSNumberInterpolationType::maybeConvertNeutral(const InterpolationValue&, ConversionCheckers&) const
{
    return createNumberValue(0);
}

InterpolationValue CSSNumberInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers& conversionCheckers) const
{
    double initialNumber;
    if (!NumberPropertyFunctions::getInitialNumber(cssProperty(), initialNumber))
        return nullptr;
    return createNumberValue(initialNumber);
}

InterpolationValue CSSNumberInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (!state.parentStyle())
        return nullptr;
    double inheritedNumber;
    if (!NumberPropertyFunctions::getNumber(cssProperty(), *state.parentStyle(), inheritedNumber))
        return nullptr;
    conversionCheckers.append(ParentNumberChecker::create(cssProperty(), inheritedNumber));
    return createNumberValue(inheritedNumber);
}

InterpolationValue CSSNumberInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (!value.isPrimitiveValue() || !toCSSPrimitiveValue(value).isNumber())
        return nullptr;
    return createNumberValue(toCSSPrimitiveValue(value).getDoubleValue());
}

InterpolationValue CSSNumberInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    double underlyingNumber;
    if (!NumberPropertyFunctions::getNumber(cssProperty(), *environment.state().style(), underlyingNumber))
        return nullptr;
    return createNumberValue(underlyingNumber);
}

void CSSNumberInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue*, InterpolationEnvironment& environment) const
{
    double clampedNumber = NumberPropertyFunctions::clampNumber(cssProperty(), toInterpolableNumber(interpolableValue).value());
    if (!NumberPropertyFunctions::setNumber(cssProperty(), *environment.state().style(), clampedNumber))
        StyleBuilder::applyProperty(cssProperty(), environment.state(), *CSSPrimitiveValue::create(clampedNumber, CSSPrimitiveValue::UnitType::Number));
}

} // namespace blink
