// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSClipInterpolationType.h"

#include "core/animation/CSSLengthInterpolationType.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

struct ClipAutos {
    ClipAutos()
        : isAuto(true)
        , isTopAuto(false)
        , isRightAuto(false)
        , isBottomAuto(false)
        , isLeftAuto(false)
    { }
    ClipAutos(bool isTopAuto, bool isRightAuto, bool isBottomAuto, bool isLeftAuto)
        : isAuto(false)
        , isTopAuto(isTopAuto)
        , isRightAuto(isRightAuto)
        , isBottomAuto(isBottomAuto)
        , isLeftAuto(isLeftAuto)
    { }
    explicit ClipAutos(const LengthBox& clip)
        : isAuto(false)
        , isTopAuto(clip.top().isAuto())
        , isRightAuto(clip.right().isAuto())
        , isBottomAuto(clip.bottom().isAuto())
        , isLeftAuto(clip.left().isAuto())
    { }

    bool operator==(const ClipAutos& other) const
    {
        return isAuto == other.isAuto
            && isTopAuto == other.isTopAuto
            && isRightAuto == other.isRightAuto
            && isBottomAuto == other.isBottomAuto
            && isLeftAuto == other.isLeftAuto;
    }
    bool operator!=(const ClipAutos& other) const { return !(*this == other); }

    bool isAuto;
    bool isTopAuto;
    bool isRightAuto;
    bool isBottomAuto;
    bool isLeftAuto;
};

static ClipAutos getClipAutos(const ComputedStyle& style)
{
    if (style.hasAutoClip())
        return ClipAutos();
    return ClipAutos(
        style.clipTop().isAuto(),
        style.clipRight().isAuto(),
        style.clipBottom().isAuto(),
        style.clipLeft().isAuto());
}

class ParentAutosChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<ParentAutosChecker> create(const ClipAutos& parentAutos)
    {
        return wrapUnique(new ParentAutosChecker(parentAutos));
    }

private:
    ParentAutosChecker(const ClipAutos& parentAutos)
        : m_parentAutos(parentAutos)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        return m_parentAutos == getClipAutos(*environment.state().parentStyle());
    }

    const ClipAutos m_parentAutos;
};

class CSSClipNonInterpolableValue : public NonInterpolableValue {
public:
    ~CSSClipNonInterpolableValue() final { }

    static PassRefPtr<CSSClipNonInterpolableValue> create(const ClipAutos& clipAutos)
    {
        return adoptRef(new CSSClipNonInterpolableValue(clipAutos));
    }

    const ClipAutos& clipAutos() const { return m_clipAutos; }

    DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

private:
    CSSClipNonInterpolableValue(const ClipAutos& clipAutos)
        : m_clipAutos(clipAutos)
    {
        ASSERT(!m_clipAutos.isAuto);
    }

    const ClipAutos m_clipAutos;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSClipNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSClipNonInterpolableValue);

class UnderlyingAutosChecker : public InterpolationType::ConversionChecker {
public:
    ~UnderlyingAutosChecker() final {}

    static std::unique_ptr<UnderlyingAutosChecker> create(const ClipAutos& underlyingAutos)
    {
        return wrapUnique(new UnderlyingAutosChecker(underlyingAutos));
    }

    static ClipAutos getUnderlyingAutos(const InterpolationValue& underlying)
    {
        if (!underlying)
            return ClipAutos();
        return toCSSClipNonInterpolableValue(*underlying.nonInterpolableValue).clipAutos();
    }

private:
    UnderlyingAutosChecker(const ClipAutos& underlyingAutos)
        : m_underlyingAutos(underlyingAutos)
    { }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        return m_underlyingAutos == getUnderlyingAutos(underlying);
    }

    const ClipAutos m_underlyingAutos;
};

enum ClipComponentIndex {
    ClipTop,
    ClipRight,
    ClipBottom,
    ClipLeft,
    ClipComponentIndexCount,
};

static std::unique_ptr<InterpolableValue> convertClipComponent(const Length& length, double zoom)
{
    if (length.isAuto())
        return InterpolableList::create(0);
    return CSSLengthInterpolationType::maybeConvertLength(length, zoom).interpolableValue;
}

static InterpolationValue createClipValue(const LengthBox& clip, double zoom)
{
    std::unique_ptr<InterpolableList> list = InterpolableList::create(ClipComponentIndexCount);
    list->set(ClipTop, convertClipComponent(clip.top(), zoom));
    list->set(ClipRight, convertClipComponent(clip.right(), zoom));
    list->set(ClipBottom, convertClipComponent(clip.bottom(), zoom));
    list->set(ClipLeft, convertClipComponent(clip.left(), zoom));
    return InterpolationValue(std::move(list), CSSClipNonInterpolableValue::create(ClipAutos(clip)));
}

InterpolationValue CSSClipInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    ClipAutos underlyingAutos = UnderlyingAutosChecker::getUnderlyingAutos(underlying);
    conversionCheckers.append(UnderlyingAutosChecker::create(underlyingAutos));
    if (underlyingAutos.isAuto)
        return nullptr;
    LengthBox neutralBox(
        underlyingAutos.isTopAuto ? Length(Auto) : Length(0, Fixed),
        underlyingAutos.isRightAuto ? Length(Auto) : Length(0, Fixed),
        underlyingAutos.isBottomAuto ? Length(Auto) : Length(0, Fixed),
        underlyingAutos.isLeftAuto ? Length(Auto) : Length(0, Fixed));
    return createClipValue(neutralBox, 1);
}

InterpolationValue CSSClipInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers&) const
{
    return nullptr;
}

InterpolationValue CSSClipInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    ClipAutos parentAutos = getClipAutos(*state.parentStyle());
    conversionCheckers.append(ParentAutosChecker::create(parentAutos));
    if (parentAutos.isAuto)
        return nullptr;
    return createClipValue(state.parentStyle()->clip(), state.parentStyle()->effectiveZoom());
}

static bool isCSSAuto(const CSSPrimitiveValue& value)
{
    return value.getValueID() == CSSValueAuto;
}

static std::unique_ptr<InterpolableValue> convertClipComponent(const CSSPrimitiveValue& length)
{
    if (isCSSAuto(length))
        return InterpolableList::create(0);
    return CSSLengthInterpolationType::maybeConvertCSSValue(length).interpolableValue;
}

InterpolationValue CSSClipInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState& state, ConversionCheckers&) const
{
    if (!value.isQuadValue())
        return nullptr;
    const CSSQuadValue& quad = toCSSQuadValue(value);
    std::unique_ptr<InterpolableList> list = InterpolableList::create(ClipComponentIndexCount);
    list->set(ClipTop, convertClipComponent(*quad.top()));
    list->set(ClipRight, convertClipComponent(*quad.right()));
    list->set(ClipBottom, convertClipComponent(*quad.bottom()));
    list->set(ClipLeft, convertClipComponent(*quad.left()));
    ClipAutos autos(
        isCSSAuto(*quad.top()),
        isCSSAuto(*quad.right()),
        isCSSAuto(*quad.bottom()),
        isCSSAuto(*quad.left()));
    return InterpolationValue(std::move(list), CSSClipNonInterpolableValue::create(autos));
}

InterpolationValue CSSClipInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    if (environment.state().style()->hasAutoClip())
        return nullptr;
    return createClipValue(environment.state().style()->clip(), environment.state().style()->effectiveZoom());
}

PairwiseInterpolationValue CSSClipInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    const ClipAutos& startAutos = toCSSClipNonInterpolableValue(*start.nonInterpolableValue).clipAutos();
    const ClipAutos& endAutos = toCSSClipNonInterpolableValue(*end.nonInterpolableValue).clipAutos();
    if (startAutos != endAutos)
        return nullptr;
    return PairwiseInterpolationValue(std::move(start.interpolableValue), std::move(end.interpolableValue), start.nonInterpolableValue.release());
}

void CSSClipInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    const ClipAutos& underlyingAutos = toCSSClipNonInterpolableValue(*underlyingValueOwner.value().nonInterpolableValue).clipAutos();
    const ClipAutos& autos = toCSSClipNonInterpolableValue(*value.nonInterpolableValue).clipAutos();
    if (underlyingAutos == autos)
        underlyingValueOwner.mutableValue().interpolableValue->scaleAndAdd(underlyingFraction, *value.interpolableValue);
    else
        underlyingValueOwner.set(*this, value);
}

void CSSClipInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    const ClipAutos& autos = toCSSClipNonInterpolableValue(nonInterpolableValue)->clipAutos();
    const InterpolableList& list = toInterpolableList(interpolableValue);
    const auto& convertIndex = [&list, &environment](bool isAuto, size_t index)
    {
        if (isAuto)
            return Length(Auto);
        return CSSLengthInterpolationType::resolveInterpolableLength(*list.get(index), nullptr, environment.state().cssToLengthConversionData(), ValueRangeAll);
    };
    environment.state().style()->setClip(LengthBox(
        convertIndex(autos.isTopAuto, ClipTop),
        convertIndex(autos.isRightAuto, ClipRight),
        convertIndex(autos.isBottomAuto, ClipBottom),
        convertIndex(autos.isLeftAuto, ClipLeft)));
}

} // namespace blink
