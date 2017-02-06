// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSFilterListInterpolationType.h"

#include "core/animation/FilterInterpolationFunctions.h"
#include "core/animation/FilterListPropertyFunctions.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

class UnderlyingFilterListChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<UnderlyingFilterListChecker> create(PassRefPtr<NonInterpolableList> nonInterpolableList)
    {
        return wrapUnique(new UnderlyingFilterListChecker(nonInterpolableList));
    }

    bool isValid(const InterpolationEnvironment&, const InterpolationValue& underlying) const final
    {
        const NonInterpolableList& underlyingNonInterpolableList = toNonInterpolableList(*underlying.nonInterpolableValue);
        if (m_nonInterpolableList->length() != underlyingNonInterpolableList.length())
            return false;
        for (size_t i = 0; i < m_nonInterpolableList->length(); i++) {
            if (!FilterInterpolationFunctions::filtersAreCompatible(*m_nonInterpolableList->get(i), *underlyingNonInterpolableList.get(i)))
                return false;
        }
        return true;
    }

private:
    UnderlyingFilterListChecker(PassRefPtr<NonInterpolableList> nonInterpolableList)
        : m_nonInterpolableList(nonInterpolableList)
    { }

    RefPtr<NonInterpolableList> m_nonInterpolableList;
};

class InheritedFilterListChecker : public InterpolationType::ConversionChecker {
public:
    static std::unique_ptr<InheritedFilterListChecker> create(CSSPropertyID property, const FilterOperations& filterOperations)
    {
        return wrapUnique(new InheritedFilterListChecker(property, filterOperations));
    }

    bool isValid(const InterpolationEnvironment& environment, const InterpolationValue&) const final
    {
        const FilterOperations& filterOperations = m_filterOperationsWrapper->operations();
        return filterOperations == FilterListPropertyFunctions::getFilterList(m_property, *environment.state().parentStyle());
    }

private:
    InheritedFilterListChecker(CSSPropertyID property, const FilterOperations& filterOperations)
        : m_property(property)
        , m_filterOperationsWrapper(FilterOperationsWrapper::create(filterOperations))
    { }

    const CSSPropertyID m_property;
    Persistent<FilterOperationsWrapper> m_filterOperationsWrapper;
};

InterpolationValue convertFilterList(const FilterOperations& filterOperations, double zoom)
{
    size_t length = filterOperations.size();
    std::unique_ptr<InterpolableList> interpolableList = InterpolableList::create(length);
    Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(length);
    for (size_t i = 0; i < length; i++) {
        InterpolationValue filterResult = FilterInterpolationFunctions::maybeConvertFilter(*filterOperations.operations()[i], zoom);
        if (!filterResult)
            return nullptr;
        interpolableList->set(i, std::move(filterResult.interpolableValue));
        nonInterpolableValues[i] = filterResult.nonInterpolableValue.release();
    }
    return InterpolationValue(std::move(interpolableList), NonInterpolableList::create(std::move(nonInterpolableValues)));
}

} // namespace

InterpolationValue CSSFilterListInterpolationType::maybeConvertNeutral(const InterpolationValue& underlying, ConversionCheckers& conversionCheckers) const
{
    // const_cast for taking refs.
    NonInterpolableList& nonInterpolableList = const_cast<NonInterpolableList&>(toNonInterpolableList(*underlying.nonInterpolableValue));
    conversionCheckers.append(UnderlyingFilterListChecker::create(&nonInterpolableList));
    return InterpolationValue(underlying.interpolableValue->cloneAndZero(), &nonInterpolableList);
}

InterpolationValue CSSFilterListInterpolationType::maybeConvertInitial(const StyleResolverState&, ConversionCheckers& conversionCheckers) const
{
    return convertFilterList(FilterListPropertyFunctions::getInitialFilterList(cssProperty()), 1);
}

InterpolationValue CSSFilterListInterpolationType::maybeConvertInherit(const StyleResolverState& state, ConversionCheckers& conversionCheckers) const
{
    const FilterOperations& inheritedFilterOperations = FilterListPropertyFunctions::getFilterList(cssProperty(), *state.parentStyle());
    conversionCheckers.append(InheritedFilterListChecker::create(cssProperty(), inheritedFilterOperations));
    return convertFilterList(inheritedFilterOperations, state.style()->effectiveZoom());
}

InterpolationValue CSSFilterListInterpolationType::maybeConvertValue(const CSSValue& value, const StyleResolverState&, ConversionCheckers&) const
{
    if (value.isPrimitiveValue() && toCSSPrimitiveValue(value).getValueID() == CSSValueNone)
        return InterpolationValue(InterpolableList::create(0), NonInterpolableList::create());

    if (!value.isBaseValueList())
        return nullptr;

    const CSSValueList& list = toCSSValueList(value);
    size_t length = list.length();
    std::unique_ptr<InterpolableList> interpolableList = InterpolableList::create(length);
    Vector<RefPtr<NonInterpolableValue>> nonInterpolableValues(length);
    for (size_t i = 0; i < length; i++) {
        InterpolationValue itemResult = FilterInterpolationFunctions::maybeConvertCSSFilter(list.item(i));
        if (!itemResult)
            return nullptr;
        interpolableList->set(i, std::move(itemResult.interpolableValue));
        nonInterpolableValues[i] = itemResult.nonInterpolableValue.release();
    }
    return InterpolationValue(std::move(interpolableList), NonInterpolableList::create(std::move(nonInterpolableValues)));
}

InterpolationValue CSSFilterListInterpolationType::maybeConvertUnderlyingValue(const InterpolationEnvironment& environment) const
{
    const ComputedStyle& style = *environment.state().style();
    return convertFilterList(FilterListPropertyFunctions::getFilterList(cssProperty(), style), style.effectiveZoom());
}

PairwiseInterpolationValue CSSFilterListInterpolationType::maybeMergeSingles(InterpolationValue&& start, InterpolationValue&& end) const
{
    NonInterpolableList& startNonInterpolableList = toNonInterpolableList(*start.nonInterpolableValue);
    NonInterpolableList& endNonInterpolableList = toNonInterpolableList(*end.nonInterpolableValue);
    size_t startLength = startNonInterpolableList.length();
    size_t endLength = endNonInterpolableList.length();

    for (size_t i = 0; i < startLength && i < endLength; i++) {
        if (!FilterInterpolationFunctions::filtersAreCompatible(*startNonInterpolableList.get(i), *endNonInterpolableList.get(i)))
            return nullptr;
    }

    if (startLength == endLength)
        return PairwiseInterpolationValue(std::move(start.interpolableValue), std::move(end.interpolableValue), start.nonInterpolableValue.release());

    // Extend the shorter InterpolableList with neutral values that are compatible with corresponding filters in the longer list.
    InterpolationValue& shorter = startLength < endLength ? start : end;
    InterpolationValue& longer = startLength < endLength ? end : start;
    size_t shorterLength = toNonInterpolableList(*shorter.nonInterpolableValue).length();
    size_t longerLength = toNonInterpolableList(*longer.nonInterpolableValue).length();
    InterpolableList& shorterInterpolableList = toInterpolableList(*shorter.interpolableValue);
    const NonInterpolableList& longerNonInterpolableList = toNonInterpolableList(*longer.nonInterpolableValue);
    std::unique_ptr<InterpolableList> extendedInterpolableList = InterpolableList::create(longerLength);
    for (size_t i = 0; i < longerLength; i++) {
        if (i < shorterLength)
            extendedInterpolableList->set(i, std::move(shorterInterpolableList.getMutable(i)));
        else
            extendedInterpolableList->set(i, FilterInterpolationFunctions::createNoneValue(*longerNonInterpolableList.get(i)));
    }
    shorter.interpolableValue = std::move(extendedInterpolableList);

    return PairwiseInterpolationValue(std::move(start.interpolableValue), std::move(end.interpolableValue), longer.nonInterpolableValue.release());
}

void CSSFilterListInterpolationType::composite(UnderlyingValueOwner& underlyingValueOwner, double underlyingFraction, const InterpolationValue& value, double interpolationFraction) const
{
    const NonInterpolableList& underlyingNonInterpolableList = toNonInterpolableList(*underlyingValueOwner.value().nonInterpolableValue);
    const NonInterpolableList& nonInterpolableList = toNonInterpolableList(*value.nonInterpolableValue);
    size_t underlyingLength = underlyingNonInterpolableList.length();
    size_t length = nonInterpolableList.length();

    for (size_t i = 0; i < underlyingLength && i < length; i++) {
        if (!FilterInterpolationFunctions::filtersAreCompatible(*underlyingNonInterpolableList.get(i), *nonInterpolableList.get(i))) {
            underlyingValueOwner.set(*this, value);
            return;
        }
    }

    InterpolableList& underlyingInterpolableList = toInterpolableList(*underlyingValueOwner.mutableValue().interpolableValue);
    const InterpolableList& interpolableList = toInterpolableList(*value.interpolableValue);
    ASSERT(underlyingLength == underlyingInterpolableList.length());
    ASSERT(length == interpolableList.length());

    for (size_t i = 0; i < length && i < underlyingLength; i++)
        underlyingInterpolableList.getMutable(i)->scaleAndAdd(underlyingFraction, *interpolableList.get(i));

    if (length <= underlyingLength)
        return;

    std::unique_ptr<InterpolableList> extendedInterpolableList = InterpolableList::create(length);
    for (size_t i = 0; i < length; i++) {
        if (i < underlyingLength)
            extendedInterpolableList->set(i, std::move(underlyingInterpolableList.getMutable(i)));
        else
            extendedInterpolableList->set(i, interpolableList.get(i)->clone());
    }
    underlyingValueOwner.mutableValue().interpolableValue = std::move(extendedInterpolableList);
    // const_cast to take a ref.
    underlyingValueOwner.mutableValue().nonInterpolableValue = const_cast<NonInterpolableValue*>(value.nonInterpolableValue.get());
}

void CSSFilterListInterpolationType::apply(const InterpolableValue& interpolableValue, const NonInterpolableValue* nonInterpolableValue, InterpolationEnvironment& environment) const
{
    const InterpolableList& interpolableList = toInterpolableList(interpolableValue);
    const NonInterpolableList& nonInterpolableList = toNonInterpolableList(*nonInterpolableValue);
    size_t length = interpolableList.length();
    ASSERT(length == nonInterpolableList.length());

    FilterOperations filterOperations;
    filterOperations.operations().reserveCapacity(length);
    for (size_t i = 0; i < length; i++)
        filterOperations.operations().append(FilterInterpolationFunctions::createFilter(*interpolableList.get(i), *nonInterpolableList.get(i), environment.state()));
    FilterListPropertyFunctions::setFilterList(cssProperty(), *environment.state().style(), std::move(filterOperations));
}

} // namespace blink
