// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSBorderImageLengthBoxInterpolationType.h"

#include "core/animation/BorderImageLengthBoxPropertyFunctions.h"
#include "core/animation/CSSLengthInterpolationType.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

enum SideIndex {
    SideTop,
    SideRight,
    SideBottom,
    SideLeft,
    SideIndexCount,
};

struct SideNumbers {
    explicit SideNumbers(const BorderImageLengthBox& box)
    {
        isNumber[SideTop] = box.top().isNumber();
        isNumber[SideRight] = box.right().isNumber();
        isNumber[SideBottom] = box.bottom().isNumber();
        isNumber[SideLeft] = box.left().isNumber();
    }
    explicit SideNumbers(const CSSQuadValue& quad)
    {
        isNumber[SideTop] = quad.top()->isNumber();
        isNumber[SideRight] = quad.right()->isNumber();
        isNumber[SideBottom] = quad.bottom()->isNumber();
        isNumber[SideLeft] = quad.left()->isNumber();
    }

    bool operator==(const SideNumbers& other) const
    {
        for (size_t i = 0; i < SideIndexCount; i++) {
            if (isNumber[i] != other.isNumber[i])
                return false;
        }
        return true;
    }
    bool operator!=(const SideNumbers& other) const { return !(*this == other); }

    bool isNumber[SideIndexCount];
};

} // namespace

class CSSBorderImageLengthBoxNonInterpolableValue : public NonInterpolableValue {
public:
    static PassRefPtr<CSSBorderImageLengthBoxNonInterpolableValue> create(const SideNumbers& sideNumbers, Vector<RefPtr<NonInterpolableValue>>&& sideNonInterpolableValues)
    {
        return adoptRef(new CSSBorderImageLengthBoxNonInterpolableValue(sideNumbers, std::move(sideNonInterpolableValues)));
    }

    const SideNumbers& sideNumbers() const { return m_sideNumbers; }
    const Vector<RefPtr<NonInterpolableValue>>& sideNonInterpolableValues() const { return m_sideNonInterpolableValues; }
    Vector<RefPtr<NonInterpolableValue>>& sideNonInterpolableValues() { return m_sideNonInterpolableValues; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    CSSBorderImageLengthBoxNonInterpolableValue(const SideNumbers& sideNumbers, Vector<RefPtr<NonInterpolableValue>>&& sideNonInterpolableValues)
        : m_sideNumbers(sideNumbers)
        , m_sideNonInterpolableValues(sideNonInterpolableValues)
    {
        ASSERT(m_sideNonInterpolableValues.size() == SideIndexCount);
    }

    const SideNumbers m_sideNumbers;
    Vector<RefPtr<NonInterpolableValue>> m_sideNonInterpolableValues;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSBorderImageLengthBoxNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSBorderImageLengthBoxNonInterpolableValue);

namespace {

class UnderlyingSideNumbersChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<UnderlyingSideNumbersChecker> create(const SideNumbers& underlyingSideNumbers)
    {
        return wrapUnique(new UnderlyingSideNumbersChecker(underlyingSideNumbers));
    }

    static SideNumbers getUnderlyingSideNumbers(const InterpolationValue& underlying)
    {
        return toCSSBorderImageLengthBoxNonInterpolableValue(*underlying.nonInterpolableValue).sideNumbers();
    }

private:
    UnderlyingSideNumbersChecker(const SideNumbers& underlyingSideNumbers)
        : m_underlyingSideNumbers(underlyingSideNumbers)
    { }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        return m_underlyingSideNumbers == getUnderlyingSideNumbers(underlying);
    }

    const SideNumbers m_underlyingSideNumbers;
};

class InheritedSideNumbersChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<InheritedSideNumbersChecker> create(CSSPropertyID property, const SideNumbers& inheritedSideNumbers)
    {
        return wrapUnique(new InheritedSideNumbersChecker(property, inheritedSideNumbers));
    }

private:
    InheritedSideNumbersChecker(CSSPropertyID property, const SideNumbers& inheritedSideNumbers)
        : m_property(property)
        , m_inheritedSideNumbers(inheritedSideNumbers)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        return m_inheritedSideNumbers == SideNumbers(BorderImageLengthBoxPropertyFunctions::getBorderImageLengthBox(m_property, *environment.state().parentStyle()));
    }

    const CSSPropertyID m_property;
    const SideNumbers m_inheritedSideNumbers;
};

InterpolationValue convertBorderImageLengthBox(const BorderImageLengthBox& box, double zoom)
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(SideIndexCount);
    Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(SideIndexCount);
    const BorderImageLength* sides[SideIndexCount] = {};
    sides[SideTop] = &box.top();
    sides[SideRight] = &box.right();
    sides[SideBottom] = &box.bottom();
    sides[SideLeft] = &box.left();

    for (size_t i = 0; i < SideIndexCount; i++) {
        const BorderImageLength& side = *sides[i];
        if (side.isNumber()) {
            list->set(i, InterpolableNumber::create(side.number()));
        } else {
            InterpolationValue convertedSide = CSSLengthInterpolationType::maybeConvertLength(side.length(), zoom);
            if (!convertedSide)
                return nullptr;
            list->set(i, std::move(convertedSide.interpolableValue));
            nonInterpolableValues[i] = convertedSide.nonInterpolableValue.release();
        }
    }

    return InterpolationValue(std::move(list), CSSBorderImageLengthBoxNonInterpolableValue::create(SideNumbers(box), std::move(nonInterpolableValues)));
}

} // namespace

InterpolationValue CSSBorderImageLengthBoxInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    SideNumbers underlyingSideNumbers = UnderlyingSideNumbersChecker::getUnderlyingSideNumbers(underlying);
    conversionCheckers.append(UnderlyingSideNumbersChecker::create(underlyingSideNumbers));
    const auto& zero = [&underlyingSideNumbers](size_t index)
    {
        return underlyingSideNumbers.isNumber[index] ? BorderImageLength(0) : BorderImageLength(Length(0, Fixed));
    };
    BorderImageLengthBox zeroBox(
        zero(SideTop),
        zero(SideRight),
        zero(SideBottom),
        zero(SideLeft));
    return convertBorderImageLengthBox(zeroBox, 1);
}

InterpolationValue CSSBorderImageLengthBoxInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const
{
    return convertBorderImageLengthBox(BorderImageLengthBoxPropertyFunctions::getInitialBorderImageLengthBox(cssProperty()), 1);
}

InterpolationValue CSSBorderImageLengthBoxInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    const BorderImageLengthBox& inherited = BorderImageLengthBoxPropertyFunctions::getBorderImageLengthBox(cssProperty(), *state.parentStyle());
    conversionCheckers.append(InheritedSideNumbersChecker::create(cssProperty(), SideNumbers(inherited)));
    return convertBorderImageLengthBox(inherited, state.parentStyle()->effectiveZoom());
}

InterpolationValue CSSBorderImageLengthBoxInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (!value.isQuadValue())
        return nullptr;

    const CSSQuadValue& quad = toCSSQuadValue(value);
    std::unique_ptr<InterpolableList> list = InterpolableList::create(SideIndexCount);
    Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(SideIndexCount);
    const CSSPrimitiveValue* sides[SideIndexCount] = {};
    sides[SideTop] = quad.top();
    sides[SideRight] = quad.right();
    sides[SideBottom] = quad.bottom();
    sides[SideLeft] = quad.left();

    for (size_t i = 0; i < SideIndexCount; i++) {
        const CSSPrimitiveValue& side = *sides[i];
        if (side.isNumber()) {
            list->set(i, InterpolableNumber::create(side.getDoubleValue()));
        } else {
            InterpolationValue convertedSide = CSSLengthInterpolationType::maybeConvertCSSValue(side);
            if (!convertedSide)
                return nullptr;
            list->set(i, std::move(convertedSide.interpolableValue));
            nonInterpolableValues[i] = convertedSide.nonInterpolableValue.release();
        }
    }

    return InterpolationValue(std::move(list), CSSBorderImageLengthBoxNonInterpolableValue::create(SideNumbers(quad), std::move(nonInterpolableValues)));
}

InterpolationValue CSSBorderImageLengthBoxInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    const ComputedStyle& style = *environment.state().style();
    return convertBorderImageLengthBox(BorderImageLengthBoxPropertyFunctions::getBorderImageLengthBox(cssProperty(), style), style.effectiveZoom());
}

PairwiseInterpolationValue CSSBorderImageLengthBoxInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    const SideNumbers& startSideNumbers = toCSSBorderImageLengthBoxNonInterpolableValue(*start.nonInterpolableValue).sideNumbers();
    const SideNumbers& endSideNumbers = toCSSBorderImageLengthBoxNonInterpolableValue(*end.nonInterpolableValue).sideNumbers();

    if (startSideNumbers != endSideNumbers)
        return nullptr;

    return PairwiseInterpolationValue(std::move(start.interpolableValue), std::move(end.interpolableValue), start.nonInterpolableValue.release());
}

void CSSBorderImageLengthBoxInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    const SideNumbers& underlyingSideNumbers = toCSSBorderImageLengthBoxNonInterpolableValue(*underlyingValueOwner.value().nonInterpolableValue).sideNumbers();
    const auto& nonInterpolableValue = toCSSBorderImageLengthBoxNonInterpolableValue(*value.nonInterpolableValue);
    const SideNumbers& sideNumbers = nonInterpolableValue.sideNumbers();

    if (underlyingSideNumbers != sideNumbers) {
        underlyingValueOwner.set(*this, value);
        return;
    }

    InterpolationValue& underlyingValue = underlyingValueOwner.mutableValue();
    InterpolableList& underlyingList = toInterpolableList(*underlyingValue.interpolableValue);
    Vector<RefPtr<NonInterpolableValue>>& underlyingSideNonInterpolableValues = toCSSBorderImageLengthBoxNonInterpolableValue(*underlyingValue.nonInterpolableValue).sideNonInterpolableValues();
    const InterpolableList& list = toInterpolableList(*value.interpolableValue);
    const Vector<RefPtr<NonInterpolableValue>>& sideNonInterpolableValues = nonInterpolableValue.sideNonInterpolableValues();

    for (size_t i = 0; i < SideIndexCount; i++) {
        if (sideNumbers.isNumber[i])
            underlyingList.getMutable(i)->scaleAndAdd(underlyingFraction, *list.get(i));
        else
            CSSLengthInterpolationType::composite(underlyingList.getMutable(i), underlyingSideNonInterpolableValues[i], underlyingFraction, *list.get(i), sideNonInterpolableValues[i].get());
    }
}

void CSSBorderImageLengthBoxInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    const SideNumbers& sideNumbers = toCSSBorderImageLengthBoxNonInterpolableValue(nonInterpolableValue)->sideNumbers();
    const Vector<RefPtr<NonInterpolableValue>>& nonInterpolableValues = toCSSBorderImageLengthBoxNonInterpolableValue(nonInterpolableValue)->sideNonInterpolableValues();
    const InterpolableList& list = toInterpolableList(interpolableValue);
    const auto& convertSide = [&sideNumbers, &list, &environment, &nonInterpolableValues](size_t index) -> BorderImageLength {
        if (sideNumbers.isNumber[index])
            return clampTo<double>(toInterpolableNumber(list.get(index))->value(), 0);
        return CSSLengthInterpolationType::resolveInterpolableLength(*list.get(index), nonInterpolableValues[index].get(), environment.state().cssToLengthConversionData(), ValueRangeNonNegative);
    };
    BorderImageLengthBox box(
        convertSide(SideTop),
        convertSide(SideRight),
        convertSide(SideBottom),
        convertSide(SideLeft));
    BorderImageLengthBoxPropertyFunctions::setBorderImageLengthBox(cssProperty(), *environment.state().style(), box);
}

} // namespace blink
