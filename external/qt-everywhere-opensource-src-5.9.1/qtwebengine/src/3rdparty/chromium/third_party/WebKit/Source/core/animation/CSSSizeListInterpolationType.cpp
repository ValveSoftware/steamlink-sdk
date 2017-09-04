// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSSizeListInterpolationType.h"

#include "core/animation/ListInterpolationFunctions.h"
#include "core/animation/SizeInterpolationFunctions.h"
#include "core/animation/SizeListPropertyFunctions.h"
#include "core/css/CSSValueList.h"
#include "core/css/resolver/StyleResolverState.h"

namespace blink {

class UnderlyingSizeListChecker : public InterpolationType::ConversionChecker {
 public:
  ~UnderlyingSizeListChecker() final {}

  static std::unique_ptr<UnderlyingSizeListChecker> create(
      const NonInterpolableList& underlyingList) {
    return wrapUnique(new UnderlyingSizeListChecker(underlyingList));
  }

 private:
  UnderlyingSizeListChecker(const NonInterpolableList& underlyingList)
      : m_underlyingList(&underlyingList) {}

  bool isValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    const auto& underlyingList =
        toNonInterpolableList(*underlying.nonInterpolableValue);
    size_t underlyingLength = underlyingList.length();
    if (underlyingLength != m_underlyingList->length())
      return false;
    for (size_t i = 0; i < underlyingLength; i++) {
      bool compatible =
          SizeInterpolationFunctions::nonInterpolableValuesAreCompatible(
              underlyingList.get(i), m_underlyingList->get(i));
      if (!compatible)
        return false;
    }
    return true;
  }

  RefPtr<const NonInterpolableList> m_underlyingList;
};

class InheritedSizeListChecker : public InterpolationType::ConversionChecker {
 public:
  ~InheritedSizeListChecker() final {}

  static std::unique_ptr<InheritedSizeListChecker> create(
      CSSPropertyID property,
      const SizeList& inheritedSizeList) {
    return wrapUnique(
        new InheritedSizeListChecker(property, inheritedSizeList));
  }

 private:
  InheritedSizeListChecker(CSSPropertyID property,
                           const SizeList& inheritedSizeList)
      : m_property(property), m_inheritedSizeList(inheritedSizeList) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue&) const final {
    return m_inheritedSizeList ==
           SizeListPropertyFunctions::getSizeList(
               m_property, *environment.state().parentStyle());
  }

  CSSPropertyID m_property;
  SizeList m_inheritedSizeList;
};

InterpolationValue convertSizeList(const SizeList& sizeList, float zoom) {
  // Flatten pairs of width/height into individual items, even for contain and
  // cover keywords.
  return ListInterpolationFunctions::createList(
      sizeList.size() * 2,
      [&sizeList, zoom](size_t index) -> InterpolationValue {
        bool convertWidth = index % 2 == 0;
        return SizeInterpolationFunctions::convertFillSizeSide(
            sizeList[index / 2], zoom, convertWidth);
      });
}

InterpolationValue maybeConvertCSSSizeList(const CSSValue& value) {
  // CSSPropertyParser doesn't put single values in lists so wrap it up in a
  // temporary list.
  const CSSValueList* list = nullptr;
  if (!value.isBaseValueList()) {
    CSSValueList* tempList = CSSValueList::createCommaSeparated();
    tempList->append(value);
    list = tempList;
  } else {
    list = toCSSValueList(&value);
  }

  // Flatten pairs of width/height into individual items, even for contain and
  // cover keywords.
  return ListInterpolationFunctions::createList(
      list->length() * 2, [list](size_t index) -> InterpolationValue {
        const CSSValue& cssSize = list->item(index / 2);
        bool convertWidth = index % 2 == 0;
        return SizeInterpolationFunctions::maybeConvertCSSSizeSide(
            cssSize, convertWidth);
      });
}

InterpolationValue CSSSizeListInterpolationType::maybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  const auto& underlyingList =
      toNonInterpolableList(*underlying.nonInterpolableValue);
  conversionCheckers.append(UnderlyingSizeListChecker::create(underlyingList));
  return ListInterpolationFunctions::createList(
      underlyingList.length(), [&underlyingList](size_t index) {
        return SizeInterpolationFunctions::createNeutralValue(
            underlyingList.get(index));
      });
}

InterpolationValue CSSSizeListInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers&) const {
  return convertSizeList(
      SizeListPropertyFunctions::getInitialSizeList(cssProperty()), 1);
}

InterpolationValue CSSSizeListInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  SizeList inheritedSizeList = SizeListPropertyFunctions::getSizeList(
      cssProperty(), *state.parentStyle());
  conversionCheckers.append(
      InheritedSizeListChecker::create(cssProperty(), inheritedSizeList));
  return convertSizeList(inheritedSizeList, state.style()->effectiveZoom());
}

InterpolationValue CSSSizeListInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  return maybeConvertCSSSizeList(value);
}

PairwiseInterpolationValue CSSSizeListInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return ListInterpolationFunctions::maybeMergeSingles(
      std::move(start), std::move(end),
      SizeInterpolationFunctions::maybeMergeSingles);
}

InterpolationValue CSSSizeListInterpolationType::maybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  const ComputedStyle& style = *environment.state().style();
  return convertSizeList(
      SizeListPropertyFunctions::getSizeList(cssProperty(), style),
      style.effectiveZoom());
}

void CSSSizeListInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  ListInterpolationFunctions::composite(
      underlyingValueOwner, underlyingFraction, *this, value,
      SizeInterpolationFunctions::nonInterpolableValuesAreCompatible,
      SizeInterpolationFunctions::composite);
}

void CSSSizeListInterpolationType::apply(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    InterpolationEnvironment& environment) const {
  const auto& interpolableList = toInterpolableList(interpolableValue);
  const auto& nonInterpolableList =
      toNonInterpolableList(*nonInterpolableValue);
  size_t length = interpolableList.length();
  DCHECK_EQ(length, nonInterpolableList.length());
  DCHECK_EQ(length % 2, 0ul);
  size_t sizeListLength = length / 2;
  SizeList sizeList(sizeListLength);
  for (size_t i = 0; i < sizeListLength; i++) {
    sizeList[i] = SizeInterpolationFunctions::createFillSize(
        *interpolableList.get(i * 2), nonInterpolableList.get(i * 2),
        *interpolableList.get(i * 2 + 1), nonInterpolableList.get(i * 2 + 1),
        environment.state().cssToLengthConversionData());
  }
  SizeListPropertyFunctions::setSizeList(
      cssProperty(), *environment.state().style(), sizeList);
}

}  // namespace blink
