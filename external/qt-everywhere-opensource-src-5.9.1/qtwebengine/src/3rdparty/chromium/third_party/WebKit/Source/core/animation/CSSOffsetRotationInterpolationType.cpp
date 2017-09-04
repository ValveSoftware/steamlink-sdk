// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSOffsetRotationInterpolationType.h"

#include "core/css/resolver/StyleBuilderConverter.h"
#include "core/style/StyleOffsetRotation.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

class CSSOffsetRotationNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSOffsetRotationNonInterpolableValue() {}

  static PassRefPtr<CSSOffsetRotationNonInterpolableValue> create(
      OffsetRotationType rotationType) {
    return adoptRef(new CSSOffsetRotationNonInterpolableValue(rotationType));
  }

  OffsetRotationType rotationType() const { return m_rotationType; }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSOffsetRotationNonInterpolableValue(OffsetRotationType rotationType)
      : m_rotationType(rotationType) {}

  OffsetRotationType m_rotationType;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSOffsetRotationNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSOffsetRotationNonInterpolableValue);

namespace {

class UnderlyingRotationTypeChecker
    : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<UnderlyingRotationTypeChecker> create(
      OffsetRotationType underlyingRotationType) {
    return wrapUnique(
        new UnderlyingRotationTypeChecker(underlyingRotationType));
  }

  bool isValid(const InterpolationEnvironment&,
               const InterpolationValue& underlying) const final {
    return m_underlyingRotationType ==
           toCSSOffsetRotationNonInterpolableValue(
               *underlying.nonInterpolableValue)
               .rotationType();
  }

 private:
  UnderlyingRotationTypeChecker(OffsetRotationType underlyingRotationType)
      : m_underlyingRotationType(underlyingRotationType) {}

  OffsetRotationType m_underlyingRotationType;
};

class InheritedRotationTypeChecker
    : public InterpolationType::ConversionChecker {
 public:
  static std::unique_ptr<InheritedRotationTypeChecker> create(
      OffsetRotationType inheritedRotationType) {
    return wrapUnique(new InheritedRotationTypeChecker(inheritedRotationType));
  }

  bool isValid(const InterpolationEnvironment& environment,
               const InterpolationValue& underlying) const final {
    return m_inheritedRotationType ==
           environment.state().parentStyle()->offsetRotation().type;
  }

 private:
  InheritedRotationTypeChecker(OffsetRotationType inheritedRotationType)
      : m_inheritedRotationType(inheritedRotationType) {}

  OffsetRotationType m_inheritedRotationType;
};

InterpolationValue convertOffsetRotation(const StyleOffsetRotation& rotation) {
  return InterpolationValue(
      InterpolableNumber::create(rotation.angle),
      CSSOffsetRotationNonInterpolableValue::create(rotation.type));
}

}  // namespace

InterpolationValue CSSOffsetRotationInterpolationType::maybeConvertNeutral(
    const InterpolationValue& underlying,
    ConversionCheckers& conversionCheckers) const {
  OffsetRotationType underlyingRotationType =
      toCSSOffsetRotationNonInterpolableValue(*underlying.nonInterpolableValue)
          .rotationType();
  conversionCheckers.append(
      UnderlyingRotationTypeChecker::create(underlyingRotationType));
  return convertOffsetRotation(StyleOffsetRotation(0, underlyingRotationType));
}

InterpolationValue CSSOffsetRotationInterpolationType::maybeConvertInitial(
    const StyleResolverState&,
    ConversionCheckers& conversionCheckers) const {
  return convertOffsetRotation(StyleOffsetRotation(0, OffsetRotationAuto));
}

InterpolationValue CSSOffsetRotationInterpolationType::maybeConvertInherit(
    const StyleResolverState& state,
    ConversionCheckers& conversionCheckers) const {
  OffsetRotationType inheritedRotationType =
      state.parentStyle()->offsetRotation().type;
  conversionCheckers.append(
      InheritedRotationTypeChecker::create(inheritedRotationType));
  return convertOffsetRotation(state.parentStyle()->offsetRotation());
}

InterpolationValue CSSOffsetRotationInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  return convertOffsetRotation(
      StyleBuilderConverter::convertOffsetRotation(value));
}

PairwiseInterpolationValue
CSSOffsetRotationInterpolationType::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) const {
  const OffsetRotationType& startType =
      toCSSOffsetRotationNonInterpolableValue(*start.nonInterpolableValue)
          .rotationType();
  const OffsetRotationType& endType =
      toCSSOffsetRotationNonInterpolableValue(*end.nonInterpolableValue)
          .rotationType();
  if (startType != endType)
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolableValue),
                                    std::move(end.interpolableValue),
                                    start.nonInterpolableValue.release());
}

InterpolationValue
CSSOffsetRotationInterpolationType::maybeConvertUnderlyingValue(
    const InterpolationEnvironment& environment) const {
  return convertOffsetRotation(environment.state().style()->offsetRotation());
}

void CSSOffsetRotationInterpolationType::composite(
    UnderlyingValueOwner& underlyingValueOwner,
    double underlyingFraction,
    const InterpolationValue& value,
    double interpolationFraction) const {
  const OffsetRotationType& underlyingType =
      toCSSOffsetRotationNonInterpolableValue(
          *underlyingValueOwner.value().nonInterpolableValue)
          .rotationType();
  const OffsetRotationType& rotationType =
      toCSSOffsetRotationNonInterpolableValue(*value.nonInterpolableValue)
          .rotationType();
  if (underlyingType == rotationType)
    underlyingValueOwner.mutableValue().interpolableValue->scaleAndAdd(
        underlyingFraction, *value.interpolableValue);
  else
    underlyingValueOwner.set(*this, value);
}

void CSSOffsetRotationInterpolationType::apply(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    InterpolationEnvironment& environment) const {
  environment.state().style()->setOffsetRotation(StyleOffsetRotation(
      toInterpolableNumber(interpolableValue).value(),
      toCSSOffsetRotationNonInterpolableValue(*nonInterpolableValue)
          .rotationType()));
}

}  // namespace blink
