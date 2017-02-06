// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSMotionRotationInterpolationType.h"

#include "core/css/resolver/StyleBuilderConverter.h"
#include "core/style/StyleMotionRotation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CSSMotionRotationNonInterpolableValue : public NonInterpolableValue {
public:
    ~CSSMotionRotationNonInterpolableValue() {}

    static PassRefPtr<CSSMotionRotationNonInterpolableValue> create(MotionRotationType rotationType)
    {
        return adoptRef(new CSSMotionRotationNonInterpolableValue(rotationType));
    }

    MotionRotationType rotationType() const { return m_rotationType; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    CSSMotionRotationNonInterpolableValue(MotionRotationType rotationType)
        : m_rotationType(rotationType)
    { }

    MotionRotationType m_rotationType;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSMotionRotationNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSMotionRotationNonInterpolableValue);

namespace {

class UnderlyingRotationTypeChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<UnderlyingRotationTypeChecker> create(MotionRotationType underlyingRotationType)
    {
        return wrapUnique(new UnderlyingRotationTypeChecker(underlyingRotationType));
    }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        return m_underlyingRotationType == toCSSMotionRotationNonInterpolableValue(*underlying.nonInterpolableValue).rotationType();
    }

private:
    UnderlyingRotationTypeChecker(MotionRotationType underlyingRotationType)
        : m_underlyingRotationType(underlyingRotationType)
    { }

    MotionRotationType m_underlyingRotationType;
};

class InheritedRotationTypeChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<InheritedRotationTypeChecker> create(MotionRotationType inheritedRotationType)
    {
        return wrapUnique(new InheritedRotationTypeChecker(inheritedRotationType));
    }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        return m_inheritedRotationType == environment.state().parentStyle()->motionRotation().type;
    }

private:
    InheritedRotationTypeChecker(MotionRotationType inheritedRotationType)
        : m_inheritedRotationType(inheritedRotationType)
    { }

    MotionRotationType m_inheritedRotationType;
};

InterpolationValue convertMotionRotation(const StyleMotionRotation& rotation)
{
    return InterpolationValue(
        InterpolableNumber::create(rotation.angle),
        CSSMotionRotationNonInterpolableValue::create(rotation.type));
}

} // namespace

InterpolationValue CSSMotionRotationInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    MotionRotationType underlyingRotationType = toCSSMotionRotationNonInterpolableValue(*underlying.nonInterpolableValue).rotationType();
    conversionCheckers.append(UnderlyingRotationTypeChecker::create(underlyingRotationType));
    return convertMotionRotation(StyleMotionRotation(0, underlyingRotationType));
}

InterpolationValue CSSMotionRotationInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers& conversionCheckers) const
{
    return convertMotionRotation(StyleMotionRotation(0, MotionRotationAuto));
}

InterpolationValue CSSMotionRotationInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    MotionRotationType inheritedRotationType = state.parentStyle()->motionRotation().type;
    conversionCheckers.append(InheritedRotationTypeChecker::create(inheritedRotationType));
    return convertMotionRotation(state.parentStyle()->motionRotation());
}

InterpolationValue CSSMotionRotationInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    return convertMotionRotation(StyleBuilderConverter::convertMotionRotation(value));
}

PairwiseInterpolationValue CSSMotionRotationInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    const MotionRotationType& startType = toCSSMotionRotationNonInterpolableValue(*start.nonInterpolableValue).rotationType();
    const MotionRotationType& endType = toCSSMotionRotationNonInterpolableValue(*end.nonInterpolableValue).rotationType();
    if (startType != endType)
        return nullptr;
    return PairwiseInterpolationValue(
        std::move(start.interpolableValue),
        std::move(end.interpolableValue),
        start.nonInterpolableValue.release());
}

InterpolationValue CSSMotionRotationInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    return convertMotionRotation(environment.state().style()->motionRotation());
}

void CSSMotionRotationInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    const MotionRotationType& underlyingType = toCSSMotionRotationNonInterpolableValue(*underlyingValueOwner.value().nonInterpolableValue).rotationType();
    const MotionRotationType& rotationType = toCSSMotionRotationNonInterpolableValue(*value.nonInterpolableValue).rotationType();
    if (underlyingType == rotationType)
        underlyingValueOwner.mutableValue().interpolableValue->scaleAndAdd(underlyingFraction, *value.interpolableValue);
    else
        underlyingValueOwner.set(*this, value);
}

void CSSMotionRotationInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    environment.state().style()->setMotionRotation(StyleMotionRotation(
        toInterpolableNumber(interpolableValue).value(),
        toCSSMotionRotationNonInterpolableValue(*nonInterpolableValue).rotationType()));
}

} // namespace blink
