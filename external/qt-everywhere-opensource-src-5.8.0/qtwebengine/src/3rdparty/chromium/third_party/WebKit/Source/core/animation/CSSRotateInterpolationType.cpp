// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSRotateInterpolationType.h"

#include "core/css/resolver/StyleBuilderConverter.h"
#include "platform/transforms/RotateTransformOperation.h"
#include "platform/transforms/Rotation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CSSRotateNonInterpolableValue : public NonInterpolableValue {
public:
    static PassRefPtr<CSSRotateNonInterpolableValue> create(const Rotation& rotation)
    {
        return adoptRef(new CSSRotateNonInterpolableValue(true, rotation, Rotation(), false, false));
    }

    static PassRefPtr<CSSRotateNonInterpolableValue> create(const CSSRotateNonInterpolableValue& start, const CSSRotateNonInterpolableValue& end)
    {
        return adoptRef(new CSSRotateNonInterpolableValue(false, start.rotation(), end.rotation(), start.isAdditive(), end.isAdditive()));
    }

    PassRefPtr<CSSRotateNonInterpolableValue> composite(const CSSRotateNonInterpolableValue& other, double otherProgress)
    {
        ASSERT(m_isSingle && !m_isStartAdditive);
        if (other.m_isSingle) {
            ASSERT(otherProgress == 0);
            ASSERT(other.isAdditive());
            return create(Rotation::add(rotation(), other.rotation()));
        }

        ASSERT(other.m_isStartAdditive || other.m_isEndAdditive);
        Rotation start = other.m_isStartAdditive ? Rotation::add(rotation(), other.m_start) : other.m_start;
        Rotation end = other.m_isEndAdditive ? Rotation::add(rotation(), other.m_end) : other.m_end;
        return create(Rotation::slerp(start, end, otherProgress));
    }

    void setSingleAdditive() { ASSERT(m_isSingle); m_isStartAdditive = true; }

    Rotation slerpedRotation(double progress) const
    {
        ASSERT(!m_isStartAdditive && !m_isEndAdditive);
        ASSERT(!m_isSingle || progress == 0);
        if (progress == 0)
            return m_start;
        if (progress == 1)
            return m_end;
        return Rotation::slerp(m_start, m_end, progress);
    }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    CSSRotateNonInterpolableValue(bool isSingle, const Rotation& start, const Rotation& end, bool isStartAdditive, bool isEndAdditive)
        : m_isSingle(isSingle)
        , m_start(start)
        , m_end(end)
        , m_isStartAdditive(isStartAdditive)
        , m_isEndAdditive(isEndAdditive)
    { }

    const Rotation& rotation() const { ASSERT(m_isSingle); return m_start; }
    bool isAdditive() const { ASSERT(m_isSingle); return m_isStartAdditive; }

    bool m_isSingle;
    Rotation m_start;
    Rotation m_end;
    bool m_isStartAdditive;
    bool m_isEndAdditive;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSRotateNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSRotateNonInterpolableValue);

namespace {

Rotation getRotation(const ComputedStyle& style)
{
    if (!style.rotate())
        return Rotation(FloatPoint3D(0, 0, 1), 0);
    return Rotation(style.rotate()->axis(), style.rotate()->angle());
}

InterpolationValue convertRotation(const Rotation& rotation)
{
    return InterpolationValue(InterpolableNumber::create(0), CSSRotateNonInterpolableValue::create(rotation));
}

class InheritedRotationChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<InheritedRotationChecker> create(const Rotation& inheritedRotation)
    {
        return wrapUnique(new InheritedRotationChecker(inheritedRotation));
    }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        Rotation inheritedRotation = getRotation(*environment.state().parentStyle());
        return m_inheritedRotation.axis == inheritedRotation.axis
            && m_inheritedRotation.angle == inheritedRotation.angle;
    }
private:
    InheritedRotationChecker(const Rotation& inheritedRotation)
        : m_inheritedRotation(inheritedRotation)
    { }

    const Rotation m_inheritedRotation;
};

} // namespace

InterpolationValue CSSRotateInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const
{
    return convertRotation(Rotation());
}

InterpolationValue CSSRotateInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const
{
    return convertRotation(getRotation(ComputedStyle::initialStyle()));
}

InterpolationValue CSSRotateInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    Rotation inheritedRotation = getRotation(*state.parentStyle());
    conversionCheckers.append(InheritedRotationChecker::create(inheritedRotation));
    return convertRotation(inheritedRotation);
}

InterpolationValue CSSRotateInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    return convertRotation(StyleBuilderConverter::convertRotation(value));
}

InterpolationValue CSSRotateInterpolationType::maybeConvertSingle(const PropertySpecificKeyframe& keyframe, const InterpolationEnvironment& environment, const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    InterpolationValue result = CSSInterpolationType::maybeConvertSingle(keyframe, environment, underlying, conversionCheckers);
    if (!result)
        return nullptr;
    if (keyframe.composite() != EffectModel::CompositeReplace)
        toCSSRotateNonInterpolableValue(*result.nonInterpolableValue).setSingleAdditive();
    return result;
}

PairwiseInterpolationValue CSSRotateInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    return PairwiseInterpolationValue(
        InterpolableNumber::create(0),
        InterpolableNumber::create(1),
        CSSRotateNonInterpolableValue::create(
            toCSSRotateNonInterpolableValue(*start.nonInterpolableValue),
            toCSSRotateNonInterpolableValue(*end.nonInterpolableValue)));
}

InterpolationValue CSSRotateInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    return convertRotation(getRotation(*environment.state().style()));
}

void CSSRotateInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    CSSRotateNonInterpolableValue& underlyingNonInterpolableValue = toCSSRotateNonInterpolableValue(*underlyingValueOwner.value().nonInterpolableValue);
    const CSSRotateNonInterpolableValue& nonInterpolableValue = toCSSRotateNonInterpolableValue(*value.nonInterpolableValue);
    double progress = toInterpolableNumber(*value.interpolableValue).value();
    underlyingValueOwner.mutableValue().nonInterpolableValue = underlyingNonInterpolableValue.composite(nonInterpolableValue, progress);
}

void CSSRotateInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* untypedNonInterpolableValue, InterpolationEnvironment& environment) const
{
    double progress = toInterpolableNumber(interpolableValue).value();
    const CSSRotateNonInterpolableValue& nonInterpolableValue = toCSSRotateNonInterpolableValue(*untypedNonInterpolableValue);
    Rotation rotation = nonInterpolableValue.slerpedRotation(progress);
    environment.state().style()->setRotate(RotateTransformOperation::create(rotation, TransformOperation::Rotate3D));
}


} // namespace blink
