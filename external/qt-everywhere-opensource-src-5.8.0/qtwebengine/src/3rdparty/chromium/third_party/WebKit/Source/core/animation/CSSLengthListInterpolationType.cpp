// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSLengthListInterpolationType.h"

#include "core/animation/CSSLengthInterpolationType.h"
#include "core/animation/LengthListPropertyFunctions.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/animation/UnderlyingLengthChecker.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

CSSLengthListInterpolationType::CSSLengthListInterpolationType(CSSPropertyID property)
    : CSSInterpolationType(property)
    , m_valueRange(LengthListPropertyFunctions::getValueRange(property))
{
}

InterpolationValue CSSLengthListInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    size_t underlyingLength = UnderlyingLengthChecker::getUnderlyingLength(underlying);
    conversionCheckers.append(UnderlyingLengthChecker::create(underlyingLength));

    if (underlyingLength == 0)
        return nullptr;

    return ListInterpolationFunctions::createList(underlyingLength, [](size_t) {
        return InterpolationValue(CSSLengthInterpolationType::createNeutralInterpolableValue());
    });
}

static InterpolationValue maybeConvertLengthList(const Vector<Length>& lengthList, float zoom)
{
    if (lengthList.isEmpty())
        return nullptr;

    return ListInterpolationFunctions::createList(lengthList.size(), [&lengthList, zoom](size_t index) {
        return CSSLengthInterpolationType::maybeConvertLength(lengthList[index], zoom);
    });
}

InterpolationValue CSSLengthListInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers& conversionCheckers) const
{
    Vector<Length> initialLengthList;
    if (!LengthListPropertyFunctions::getInitialLengthList(cssProperty(), initialLengthList))
        return nullptr;
    return maybeConvertLengthList(initialLengthList, 1);
}

class ParentLengthListChecker : public InterpolationType::ConversionChecker {
public:
    ~ParentLengthListChecker() final {}

    static std::unique_ptr<ParentLengthListChecker> create(CSSPropertyID property, const Vector<Length>& inheritedLengthList)
    {
        return wrapUnique(new ParentLengthListChecker(property, inheritedLengthList));
    }

private:
    ParentLengthListChecker(CSSPropertyID property, const Vector<Length>& inheritedLengthList)
        : m_property(property)
        , m_inheritedLengthList(inheritedLengthList)
    { }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue& underlying) const final
    {
        Vector<Length> inheritedLengthList;
        LengthListPropertyFunctions::getLengthList(m_property, *environment.state().parentStyle(), inheritedLengthList);
        return m_inheritedLengthList == inheritedLengthList;
    }

    CSSPropertyID m_property;
    Vector<Length> m_inheritedLengthList;
};

InterpolationValue CSSLengthListInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    Vector<Length> inheritedLengthList;
    bool success = LengthListPropertyFunctions::getLengthList(cssProperty(), *state.parentStyle(), inheritedLengthList);
    conversionCheckers.append(ParentLengthListChecker::create(cssProperty(), inheritedLengthList));
    if (!success)
        return nullptr;
    return maybeConvertLengthList(inheritedLengthList, state.parentStyle()->effectiveZoom());
}

InterpolationValue CSSLengthListInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (!value.isBaseValueList())
        return nullptr;

    const CSSValueList& list = toCSSValueList(value);
    return ListInterpolationFunctions::createList(list.length(), [&list](size_t index) {
        return CSSLengthInterpolationType::maybeConvertCSSValue(list.item(index));
    });
}

PairwiseInterpolationValue CSSLengthListInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    return ListInterpolationFunctions::maybeMergeSingles(std::move(start), std::move(end), CSSLengthInterpolationType::staticMergeSingleConversions);
}

InterpolationValue CSSLengthListInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    Vector<Length> underlyingLengthList;
    if (!LengthListPropertyFunctions::getLengthList(cssProperty(), *environment.state().style(), underlyingLengthList))
        return nullptr;
    return maybeConvertLengthList(underlyingLengthList, environment.state().style()->effectiveZoom());
}

void CSSLengthListInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    ListInterpolationFunctions::composite(underlyingValueOwner, underlyingFraction, *this, value,
        CSSLengthInterpolationType::nonInterpolableValuesAreCompatible,
        CSSLengthInterpolationType::composite);
}

void CSSLengthListInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    const InterpolableList& interpolableList = toInterpolableList(interpolableValue);
    const size_t length = interpolableList.length();
    ASSERT(length > 0);
    const NonInterpolableList& nonInterpolableList = toNonInterpolableList(*nonInterpolableValue);
    ASSERT(nonInterpolableList.length() == length);
    Vector<Length> result(length);
    for (size_t i = 0; i < length; i++) {
        result[i] = CSSLengthInterpolationType::resolveInterpolableLength(
            *interpolableList.get(i),
            nonInterpolableList.get(i),
            environment.state().cssToLengthConversionData(),
            m_valueRange);
    }
    LengthListPropertyFunctions::setLengthList(cssProperty(), *environment.state().style(), std::move(result));
}

} // namespace blink
