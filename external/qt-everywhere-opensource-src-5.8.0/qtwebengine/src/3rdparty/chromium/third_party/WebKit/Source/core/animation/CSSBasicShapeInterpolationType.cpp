// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSBasicShapeInterpolationType.h"

#include "core/animation/BasicShapeInterpolationFunctions.h"
#include "core/animation/BasicShapePropertyFunctions.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/style/BasicShapes.h"
#include "core/style/DataEquivalency.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

class UnderlyingCompatibilityChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<UnderlyingCompatibilityChecker> create(PassRefPtr<NonInterpolableValue> underlyingNonInterpolableValue)
    {
        return wrapUnique(new UnderlyingCompatibilityChecker(underlyingNonInterpolableValue));
    }

private:
    UnderlyingCompatibilityChecker(PassRefPtr<NonInterpolableValue> underlyingNonInterpolableValue)
        : m_underlyingNonInterpolableValue(underlyingNonInterpolableValue)
    { }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        return BasicShapeInterpolationFunctions::shapesAreCompatible(*m_underlyingNonInterpolableValue, *underlying.nonInterpolableValue);
    }

    RefPtr<NonInterpolableValue> m_underlyingNonInterpolableValue;
};

class InheritedShapeChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<InheritedShapeChecker> create(CSSPropertyID property, PassRefPtr<BasicShape> inheritedShape)
    {
        return wrapUnique(new InheritedShapeChecker(property, inheritedShape));
    }

private:
    InheritedShapeChecker(CSSPropertyID property, PassRefPtr<BasicShape> inheritedShape)
        : m_property(property)
        , m_inheritedShape(inheritedShape)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue&) const final
    {
        return dataEquivalent(m_inheritedShape.get(), BasicShapePropertyFunctions::getBasicShape(m_property, *environment.state().parentStyle()));
    }

    const CSSPropertyID m_property;
    RefPtr<BasicShape> m_inheritedShape;
};

} // namespace

InterpolationValue CSSBasicShapeInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    // const_cast is for taking refs.
    NonInterpolableValue* nonInterpolableValue = const_cast<NonInterpolableValue*>(underlying.nonInterpolableValue.get());
    conversionCheckers.append(UnderlyingCompatibilityChecker::create(nonInterpolableValue));
    return InterpolationValue(BasicShapeInterpolationFunctions::createNeutralValue(*underlying.nonInterpolableValue), nonInterpolableValue);
}

InterpolationValue CSSBasicShapeInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const
{
    return BasicShapeInterpolationFunctions::maybeConvertBasicShape(
        BasicShapePropertyFunctions::getInitialBasicShape(cssProperty()), 1);
}

InterpolationValue CSSBasicShapeInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    const BasicShape* shape = BasicShapePropertyFunctions::getBasicShape(cssProperty(), *state.parentStyle());
    // const_cast to take a ref.
    conversionCheckers.append(InheritedShapeChecker::create(cssProperty(), const_cast<BasicShape*>(shape)));
    return BasicShapeInterpolationFunctions::maybeConvertBasicShape(shape, state.parentStyle()->effectiveZoom());
}

InterpolationValue CSSBasicShapeInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (!value.isBaseValueList())
        return BasicShapeInterpolationFunctions::maybeConvertCSSValue(value);

    const CSSValueList& list = toCSSValueList(value);
    if (list.length() != 1)
        return nullptr;
    return BasicShapeInterpolationFunctions::maybeConvertCSSValue(list.item(0));
}

PairwiseInterpolationValue CSSBasicShapeInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    if (!BasicShapeInterpolationFunctions::shapesAreCompatible(*start.nonInterpolableValue, *end.nonInterpolableValue))
        return nullptr;
    return PairwiseInterpolationValue(std::move(start.interpolableValue), std::move(end.interpolableValue), start.nonInterpolableValue.release());
}

InterpolationValue CSSBasicShapeInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    const ComputedStyle& style = *environment.state().style();
    return BasicShapeInterpolationFunctions::maybeConvertBasicShape(
        BasicShapePropertyFunctions::getBasicShape(cssProperty(), style), style.effectiveZoom());
}

void CSSBasicShapeInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    if (!BasicShapeInterpolationFunctions::shapesAreCompatible(*underlyingValueOwner.value().nonInterpolableValue, *value.nonInterpolableValue)) {
        underlyingValueOwner.set(*this, value);
        return;
    }

    underlyingValueOwner.mutableValue().interpolableValue->scaleAndAdd(underlyingFraction, *value.interpolableValue);
}

void CSSBasicShapeInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    BasicShapePropertyFunctions::setBasicShape(cssProperty(), *environment.state().style(),
        BasicShapeInterpolationFunctions::createBasicShape(interpolableValue, *nonInterpolableValue, environment.state().cssToLengthConversionData()));
}

} // namespace blink
