// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSTransformInterpolationType.h"

#include "core/animation/LengthUnitsChecker.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/css/resolver/TransformBuilder.h"
#include "platform/transforms/TransformOperations.h"
#include "platform/transforms/TranslateTransformOperation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CSSTransformNonInterpolableValue : public NonInterpolableValue {
public:
    static PassRefPtr<CSSTransformNonInterpolableValue> create(TransformOperations&& transform)
    {
        return adoptRef(new CSSTransformNonInterpolableValue(true, std::move(transform), EmptyTransformOperations(), false, false));
    }

    static PassRefPtr<CSSTransformNonInterpolableValue> create(CSSTransformNonInterpolableValue&& start, CSSTransformNonInterpolableValue&& end)
    {
        return adoptRef(new CSSTransformNonInterpolableValue(false, std::move(start.transform()), std::move(end.transform()), start.isAdditive(), end.isAdditive()));
    }

    PassRefPtr<CSSTransformNonInterpolableValue> composite(const CSSTransformNonInterpolableValue& other, double otherProgress)
    {
        ASSERT(!isAdditive());
        if (other.m_isSingle) {
            ASSERT(otherProgress == 0);
            ASSERT(other.isAdditive());
            TransformOperations result;
            result.operations() = concat(transform(), other.transform());
            return create(std::move(result));
        }

        ASSERT(other.m_isStartAdditive || other.m_isEndAdditive);
        TransformOperations start;
        start.operations() = other.m_isStartAdditive ? concat(transform(), other.m_start) : other.m_start.operations();
        TransformOperations end;
        end.operations() = other.m_isEndAdditive ? concat(transform(), other.m_end) : other.m_end.operations();
        return create(end.blend(start, otherProgress));
    }

    void setSingleAdditive() { ASSERT(m_isSingle); m_isStartAdditive = true; }

    TransformOperations getInterpolatedTransform(double progress) const
    {
        ASSERT(!m_isStartAdditive && !m_isEndAdditive);
        ASSERT(!m_isSingle || progress == 0);
        if (progress == 0)
            return m_start;
        if (progress == 1)
            return m_end;
        return m_end.blend(m_start, progress);
    }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    CSSTransformNonInterpolableValue(bool isSingle, TransformOperations&& start, TransformOperations&& end, bool isStartAdditive, bool isEndAdditive)
        : m_isSingle(isSingle)
        , m_start(std::move(start))
        , m_end(std::move(end))
        , m_isStartAdditive(isStartAdditive)
        , m_isEndAdditive(isEndAdditive)
    { }

    const TransformOperations& transform() const { ASSERT(m_isSingle); return m_start; }
    TransformOperations& transform() { ASSERT(m_isSingle); return m_start; }
    bool isAdditive() const { ASSERT(m_isSingle); return m_isStartAdditive; }

    Vector<RefPtr<TransformOperation>> concat(const TransformOperations& a, const TransformOperations& b)
    {
        Vector<RefPtr<TransformOperation>> result;
        result.reserveCapacity(a.size() + b.size());
        result.appendVector(a.operations());
        result.appendVector(b.operations());
        return result;
    }

    bool m_isSingle;
    TransformOperations m_start;
    TransformOperations m_end;
    bool m_isStartAdditive;
    bool m_isEndAdditive;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSTransformNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSTransformNonInterpolableValue);

namespace {

InterpolationValue convertTransform(TransformOperations&& transform)
{
    return InterpolationValue(InterpolableNumber::create(0), CSSTransformNonInterpolableValue::create(std::move(transform)));
}

InterpolationValue convertTransform(const TransformOperations& transform)
{
    return convertTransform(TransformOperations(transform));
}

class InheritedTransformChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<InheritedTransformChecker> create(const TransformOperations& inheritedTransform)
    {
        return wrapUnique(new InheritedTransformChecker(inheritedTransform));
    }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        return m_inheritedTransform == environment.state().parentStyle()->transform();
    }
private:
    InheritedTransformChecker(const TransformOperations& inheritedTransform)
        : m_inheritedTransform(inheritedTransform)
    { }

    const TransformOperations m_inheritedTransform;
};

} // namespace

InterpolationValue CSSTransformInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers&) const
{
    return convertTransform(EmptyTransformOperations());
}

InterpolationValue CSSTransformInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const
{
    return convertTransform(ComputedStyle::initialStyle().transform());
}

InterpolationValue CSSTransformInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    const TransformOperations& inheritedTransform = state.parentStyle()->transform();
    conversionCheckers.append(InheritedTransformChecker::create(inheritedTransform));
    return convertTransform(inheritedTransform);
}

InterpolationValue CSSTransformInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    if (value.isValueList()) {
        CSSLengthArray lengthArray;
        for (const CSSValue* item : toCSSValueList(value)) {
            const CSSFunctionValue& transformFunction = toCSSFunctionValue(*item);
            if (transformFunction.functionType() == CSSValueMatrix || transformFunction.functionType() == CSSValueMatrix3d) {
                lengthArray.typeFlags.set(CSSPrimitiveValue::UnitTypePixels);
                continue;
            }
            for (const CSSValue* argument : transformFunction) {
                const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(*argument);
                if (!primitiveValue.isLength())
                    continue;
                primitiveValue.accumulateLengthArray(lengthArray);
            }
        }
        std::unique_ptr<InterpolationType::ConversionChecker> lengthUnitsChecker = LengthUnitsChecker::maybeCreate(std::move(lengthArray), state);

        if (lengthUnitsChecker)
            conversionCheckers.append(std::move(lengthUnitsChecker));
    }

    TransformOperations transform;
    TransformBuilder::createTransformOperations(value, state.cssToLengthConversionData(), transform);
    return convertTransform(std::move(transform));
}

InterpolationValue CSSTransformInterpolationType::maybeConvertSingle(const PropertySpecificKeyframe& keyframe, const InterpolationEnvironment& environment, const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    InterpolationValue result = CSSInterpolationType::maybeConvertSingle(keyframe, environment, underlying, conversionCheckers);
    if (!result)
        return nullptr;
    if (keyframe.composite() != EffectModel::CompositeReplace)
        toCSSTransformNonInterpolableValue(*result.nonInterpolableValue).setSingleAdditive();
    return result;
}

PairwiseInterpolationValue CSSTransformInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    return PairwiseInterpolationValue(
        InterpolableNumber::create(0),
        InterpolableNumber::create(1),
        CSSTransformNonInterpolableValue::create(
            std::move(toCSSTransformNonInterpolableValue(*start.nonInterpolableValue)),
            std::move(toCSSTransformNonInterpolableValue(*end.nonInterpolableValue))));
}

InterpolationValue CSSTransformInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    return convertTransform(environment.state().style()->transform());
}

void CSSTransformInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    CSSTransformNonInterpolableValue& underlyingNonInterpolableValue = toCSSTransformNonInterpolableValue(*underlyingValueOwner.value().nonInterpolableValue);
    const CSSTransformNonInterpolableValue& nonInterpolableValue = toCSSTransformNonInterpolableValue(*value.nonInterpolableValue);
    double progress = toInterpolableNumber(*value.interpolableValue).value();
    underlyingValueOwner.mutableValue().nonInterpolableValue = underlyingNonInterpolableValue.composite(nonInterpolableValue, progress);
}

void CSSTransformInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* untypedNonInterpolableValue, InterpolationEnvironment& environment) const
{
    double progress = toInterpolableNumber(interpolableValue).value();
    const CSSTransformNonInterpolableValue& nonInterpolableValue = toCSSTransformNonInterpolableValue(*untypedNonInterpolableValue);
    environment.state().style()->setTransform(nonInterpolableValue.getInterpolatedTransform(progress));
}

} // namespace blink
