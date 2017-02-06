// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSImageSliceInterpolationType.h"

#include "core/animation/CSSLengthInterpolationType.h"
#include "core/animation/ImageSlicePropertyFunctions.h"
#include "core/css/CSSBorderImageSliceValue.h"
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

struct SliceTypes {
    explicit SliceTypes(const ImageSlice& slice)
    {
        isNumber[SideTop] = slice.slices.top().isFixed();
        isNumber[SideRight] = slice.slices.right().isFixed();
        isNumber[SideBottom] = slice.slices.bottom().isFixed();
        isNumber[SideLeft] = slice.slices.left().isFixed();
        fill = slice.fill;
    }
    explicit SliceTypes(const CSSBorderImageSliceValue& slice)
    {
        isNumber[SideTop] = slice.slices().top()->isNumber();
        isNumber[SideRight] = slice.slices().right()->isNumber();
        isNumber[SideBottom] = slice.slices().bottom()->isNumber();
        isNumber[SideLeft] = slice.slices().left()->isNumber();
        fill = slice.fill();
    }

    bool operator==(const SliceTypes& other) const
    {
        for (size_t i = 0; i < SideIndexCount; i++) {
            if (isNumber[i] != other.isNumber[i])
                return false;
        }
        return fill == other.fill;
    }
    bool operator!=(const SliceTypes& other) const { return !(*this == other); }

    // If a side is not a number then it is a percentage.
    bool isNumber[SideIndexCount];
    bool fill;
};

} // namespace

class CSSImageSliceNonInterpolableValue : public NonInterpolableValue {
public:
    static PassRefPtr<CSSImageSliceNonInterpolableValue> create(const SliceTypes& types)
    {
        return adoptRef(new CSSImageSliceNonInterpolableValue(types));
    }

    const SliceTypes& types() const { return m_types; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    CSSImageSliceNonInterpolableValue(const SliceTypes& types)
        : m_types(types)
    { }

    const SliceTypes m_types;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSImageSliceNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSImageSliceNonInterpolableValue);

namespace {

class UnderlyingSliceTypesChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<UnderlyingSliceTypesChecker> create(const SliceTypes& underlyingTypes)
    {
        return wrapUnique(new UnderlyingSliceTypesChecker(underlyingTypes));
    }

    static SliceTypes getUnderlyingSliceTypes(const InterpolationValue& underlying)
    {
        return toCSSImageSliceNonInterpolableValue(*underlying.nonInterpolableValue).types();
    }

private:
    UnderlyingSliceTypesChecker(const SliceTypes& underlyingTypes)
        : m_underlyingTypes(underlyingTypes)
    { }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        return m_underlyingTypes == getUnderlyingSliceTypes(underlying);
    }

    const SliceTypes m_underlyingTypes;
};

class InheritedSliceTypesChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<InheritedSliceTypesChecker> create(CSSPropertyID property, const SliceTypes& inheritedTypes)
    {
        return wrapUnique(new InheritedSliceTypesChecker(property, inheritedTypes));
    }

private:
    InheritedSliceTypesChecker(CSSPropertyID property, const SliceTypes& inheritedTypes)
        : m_property(property)
        , m_inheritedTypes(inheritedTypes)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        return m_inheritedTypes == SliceTypes(ImageSlicePropertyFunctions::getImageSlice(m_property, *environment.state().parentStyle()));
    }

    const CSSPropertyID m_property;
    const SliceTypes m_inheritedTypes;
};

InterpolationValue convertImageSlice(const ImageSlice& slice, double zoom)
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(SideIndexCount);
    const Length* sides[SideIndexCount] = {};
    sides[SideTop] = &slice.slices.top();
    sides[SideRight] = &slice.slices.right();
    sides[SideBottom] = &slice.slices.bottom();
    sides[SideLeft] = &slice.slices.left();

    for (size_t i = 0; i < SideIndexCount; i++) {
        const Length& side = *sides[i];
        list->set(i, InterpolableNumber::create(side.isFixed() ? side.pixels() / zoom : side.percent()));
    }

    return InterpolationValue(std::move(list), CSSImageSliceNonInterpolableValue::create(SliceTypes(slice)));
}

} // namespace

InterpolationValue CSSImageSliceInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    SliceTypes underlyingTypes = UnderlyingSliceTypesChecker::getUnderlyingSliceTypes(underlying);
    conversionCheckers.append(UnderlyingSliceTypesChecker::create(underlyingTypes));
    LengthBox zeroBox(
        Length(0, underlyingTypes.isNumber[SideTop] ? Fixed : Percent),
        Length(0, underlyingTypes.isNumber[SideRight] ? Fixed : Percent),
        Length(0, underlyingTypes.isNumber[SideBottom] ? Fixed : Percent),
        Length(0, underlyingTypes.isNumber[SideLeft] ? Fixed : Percent));
    return convertImageSlice(ImageSlice(zeroBox, underlyingTypes.fill), 1);
}

InterpolationValue CSSImageSliceInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers& conversionCheckers) const
{
    return convertImageSlice(ImageSlicePropertyFunctions::getInitialImageSlice(cssProperty()), 1);
}

InterpolationValue CSSImageSliceInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    const ImageSlice& inheritedImageSlice = ImageSlicePropertyFunctions::getImageSlice(cssProperty(), *state.parentStyle());
    conversionCheckers.append(InheritedSliceTypesChecker::create(cssProperty(), SliceTypes(inheritedImageSlice)));
    return convertImageSlice(inheritedImageSlice, state.parentStyle()->effectiveZoom());
}

InterpolationValue CSSImageSliceInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (!value.isBorderImageSliceValue())
        return nullptr;

    const CSSBorderImageSliceValue& slice = toCSSBorderImageSliceValue(value);
    std::unique_ptr<InterpolableList> list = InterpolableList::create(SideIndexCount);
    const CSSPrimitiveValue* sides[SideIndexCount];
    sides[SideTop] = slice.slices().top();
    sides[SideRight] = slice.slices().right();
    sides[SideBottom] = slice.slices().bottom();
    sides[SideLeft] = slice.slices().left();

    for (size_t i = 0; i < SideIndexCount; i++) {
        const CSSPrimitiveValue& side = *sides[i];
        ASSERT_UNUSED(side, side.isNumber() || side.isPercentage());
        list->set(i, InterpolableNumber::create(sides[i]->getDoubleValue()));
    }

    return InterpolationValue(std::move(list), CSSImageSliceNonInterpolableValue::create(SliceTypes(slice)));
}

InterpolationValue CSSImageSliceInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    const ComputedStyle& style = *environment.state().style();
    return convertImageSlice(ImageSlicePropertyFunctions::getImageSlice(cssProperty(), style), style.effectiveZoom());
}

PairwiseInterpolationValue CSSImageSliceInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    const SliceTypes& startSliceTypes = toCSSImageSliceNonInterpolableValue(*start.nonInterpolableValue).types();
    const SliceTypes& endSliceTypes = toCSSImageSliceNonInterpolableValue(*end.nonInterpolableValue).types();

    if (startSliceTypes != endSliceTypes)
        return nullptr;

    return PairwiseInterpolationValue(std::move(start.interpolableValue), std::move(end.interpolableValue), start.nonInterpolableValue.release());
}

void CSSImageSliceInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    const SliceTypes& underlyingTypes = toCSSImageSliceNonInterpolableValue(*underlyingValueOwner.value().nonInterpolableValue).types();
    const SliceTypes& types = toCSSImageSliceNonInterpolableValue(*value.nonInterpolableValue).types();

    if (underlyingTypes == types)
        underlyingValueOwner.mutableValue().interpolableValue->scaleAndAdd(underlyingFraction, *value.interpolableValue);
    else
        underlyingValueOwner.set(*this, value);
}

void CSSImageSliceInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    ComputedStyle& style = *environment.state().style();
    const InterpolableList& list = toInterpolableList(interpolableValue);
    const SliceTypes& types = toCSSImageSliceNonInterpolableValue(nonInterpolableValue)->types();
    const auto& convertSide = [&types, &list, &style](size_t index)
    {
        float value = clampTo<float>(toInterpolableNumber(list.get(index))->value(), 0);
        return types.isNumber[index] ? Length(value * style.effectiveZoom(), Fixed) : Length(value, Percent);
    };
    LengthBox box(
        convertSide(SideTop),
        convertSide(SideRight),
        convertSide(SideBottom),
        convertSide(SideLeft));
    ImageSlicePropertyFunctions::setImageSlice(cssProperty(), style, ImageSlice(box, types.fill));
}

} // namespace blink
