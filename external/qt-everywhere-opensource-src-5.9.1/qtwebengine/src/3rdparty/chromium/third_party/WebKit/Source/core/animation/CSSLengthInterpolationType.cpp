// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSLengthInterpolationType.h"

#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/LengthPropertyFunctions.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "platform/LengthFunctions.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

CSSLengthInterpolationType::CSSLengthInterpolationType(PropertyHandle property)
    : CSSInterpolationType(property),
      m_valueRange(LengthPropertyFunctions::getValueRange(cssProperty())) {}

float CSSLengthInterpolationType::effectiveZoom(
    const ComputedStyle& style) const {
  return LengthPropertyFunctions::isZoomedLength(cssProperty())
             ? style.effectiveZoom()
             : 1;
}

class InheritedLengthChecker : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedLengthChecker> create(CSSPropertyID property,
                                                        const Length& length) {
    return wrapUnique(new InheritedLengthChecker(property, length));
  }

 private:
  InheritedLengthChecker(CSSPropertyID property, const Length& length)
      : m_property(property), m_length(length) {}

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    Length parentLength;
    if (!LengthPropertyFunctions::getLength(
            m_property, *environment.state().parentStyle(), parentLength))
      return false;
    return parentLength == m_length;
  }

  const CSSPropertyID m_property;
  const Length m_length;
};

InterpolationValue CSSLengthInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(
      LengthInterpolationFunctions::createNeutralInterpolableValue());
}

InterpolationValue CSSLengthInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversionCheckers) const {
  Length initialLength;
  if (!LengthPropertyFunctions::getInitialLength(cssProperty(), initialLength))
    return nullptr;
  return LengthInterpolationFunctions::maybeConvertLength(initialLength, 1);
}

InterpolationValue CSSLengthInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  if (!state.parentStyle())
    return nullptr;
  Length inheritedLength;
  if (!LengthPropertyFunctions::getLength(cssProperty(), *state.parentStyle(),
                                          inheritedLength))
    return nullptr;
  conversionCheckers.append(
      InheritedLengthChecker::create(cssProperty(), inheritedLength));
  return LengthInterpolationFunctions::maybeConvertLength(
      inheritedLength, effectiveZoom(*state.parentStyle()));
}

InterpolationValue CSSLengthInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers& conversionCheckers) const {
  if (value.isIdentifierValue()) {
    CSSValueID valueID = toCSSIdentifierValue(value).getValueID();
    double pixels;
    if (!LengthPropertyFunctions::getPixelsForKeyword(cssProperty(), valueID,
                                                      pixels))
      return nullptr;
    return InterpolationValue(
        LengthInterpolationFunctions::createInterpolablePixels(pixels));
  }

  return LengthInterpolationFunctions::maybeConvertCSSValue(value);
}

PairwiseInterpolationValue CSSLengthInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  return LengthInterpolationFunctions::mergeSingles(std::move(start),
                                                    std::move(end));
}

InterpolationValue CSSLengthInterpolationType::maybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  Length underlyingLength;
  if (!LengthPropertyFunctions::getLength(
          cssProperty(), *environment.state().style(), underlyingLength))
    return nullptr;
  return LengthInterpolationFunctions::maybeConvertLength(
      underlyingLength, effectiveZoom(*environment.state().style()));
}

void CSSLengthInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  InterpolationValue& underlying = underlyingValueOwner.mutableValue();
  LengthInterpolationFunctions::composite(
      underlying.interpolableValue, underlying.nonInterpolableValue,
      underlyingFraction, *value.interpolableValue,
      value.nonInterpolableValue.get());
}

void CSSLengthInterpolationType::apply(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    InterpolationEnvironment& environment) const {
  StyleResolverState& state = environment.state();
  ComputedStyle& style = *state.style();
  float zoom = effectiveZoom(style);
  Length length = LengthInterpolationFunctions::createLength(
      interpolableValue, nonInterpolableValue,
      state.cssToLengthConversionData(), m_valueRange);
  if (LengthPropertyFunctions::setLength(cssProperty(), style, length)) {
#if DCHECK_IS_ON()
    // Assert that setting the length on ComputedStyle directly is identical to
    // the StyleBuilder code path. This check is useful for catching differences
    // in clamping behaviour.
    Length before;
    Length after;
    DCHECK(LengthPropertyFunctions::getLength(cssProperty(), style, before));
    StyleBuilder::applyProperty(cssProperty(), state,
                                *CSSValue::create(length, zoom));
    DCHECK(LengthPropertyFunctions::getLength(cssProperty(), style, after));
    DCHECK_EQ(before.type(), after.type());
    if (before.isSpecified()) {
      const float kSlack = 0.1;
      float delta =
          floatValueForLength(after, 100) - floatValueForLength(before, 100);
      DCHECK_LT(std::abs(delta), kSlack);
    }
#endif
    return;
  }
  StyleBuilder::applyProperty(cssProperty(), state,
                              *CSSValue::create(length, zoom));
}

}  // namespace blink
