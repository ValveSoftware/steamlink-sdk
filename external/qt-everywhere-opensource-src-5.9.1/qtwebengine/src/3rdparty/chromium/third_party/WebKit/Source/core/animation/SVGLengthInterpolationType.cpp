// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SVGLengthInterpolationType.h"

#include "core/animation/InterpolationEnvironment.h"
#include "core/animation/StringKeyframe.h"
#include "core/css/CSSHelper.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGLength.h"
#include "core/svg/SVGLengthContext.h"
#include <memory>

namespace blink {

std::unique_ptr<InterpolableValue>
SVGLengthInterpolationType::neutralInterpolableValue() {
  std::unique_ptr<InterpolableList> listOfValues =
      InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);
  for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; ++i)
    listOfValues->set(i, InterpolableNumber::create(0));

  return std::move(listOfValues);
}

InterpolationValue SVGLengthInterpolationType::convertSVGLength(
    const SVGLength& length) {
  const CSSPrimitiveValue* primitiveValue = length.asCSSPrimitiveValue();

  CSSLengthArray lengthArray;
  primitiveValue->accumulateLengthArray(lengthArray);

  std::unique_ptr<InterpolableList> listOfValues =
      InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);
  for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; ++i)
    listOfValues->set(i, InterpolableNumber::create(lengthArray.values[i]));

  return InterpolationValue(std::move(listOfValues));
}

SVGLength* SVGLengthInterpolationType::resolveInterpolableSVGLength(
    const InterpolableValue& interpolableValue,
    const SVGLengthContext& lengthContext,
    SVGLengthMode unitMode,
    bool negativeValuesForbidden) {
  const InterpolableList& listOfValues = toInterpolableList(interpolableValue);

  double value = 0;
  CSSPrimitiveValue::UnitType unitType = CSSPrimitiveValue::UnitType::UserUnits;
  unsigned unitTypeCount = 0;
  // We optimise for the common case where only one unit type is involved.
  for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
    double entry = toInterpolableNumber(listOfValues.get(i))->value();
    if (!entry)
      continue;
    unitTypeCount++;
    if (unitTypeCount > 1)
      break;

    value = entry;
    unitType = CSSPrimitiveValue::lengthUnitTypeToUnitType(
        static_cast<CSSPrimitiveValue::LengthUnitType>(i));
  }

  if (unitTypeCount > 1) {
    value = 0;
    unitType = CSSPrimitiveValue::UnitType::UserUnits;

    // SVGLength does not support calc expressions, so we convert to canonical
    // units.
    for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
      double entry = toInterpolableNumber(listOfValues.get(i))->value();
      if (entry)
        value += lengthContext.convertValueToUserUnits(
            entry, unitMode,
            CSSPrimitiveValue::lengthUnitTypeToUnitType(
                static_cast<CSSPrimitiveValue::LengthUnitType>(i)));
    }
  }

  if (negativeValuesForbidden && value < 0)
    value = 0;

  SVGLength* result = SVGLength::create(unitMode);  // defaults to the length 0
  result->newValueSpecifiedUnits(unitType, value);
  return result;
}

InterpolationValue SVGLengthInterpolationType::maybeConvertNeutral(
    const InterpolationValue&,
    ConversionCheckers&) const {
  return InterpolationValue(neutralInterpolableValue());
}

InterpolationValue SVGLengthInterpolationType::maybeConvertSVGValue(
    const SVGPropertyBase& svgValue) const {
  if (svgValue.type() != AnimatedLength)
    return nullptr;

  return convertSVGLength(toSVGLength(svgValue));
}

SVGPropertyBase* SVGLengthInterpolationType::appliedSVGValue(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue*) const {
  NOTREACHED();
  // This function is no longer called, because apply has been overridden.
  return nullptr;
}

void SVGLengthInterpolationType::apply(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    InterpolationEnvironment& environment) const {
  SVGElement& element = environment.svgElement();
  SVGLengthContext lengthContext(&element);
  element.setWebAnimatedAttribute(
      attribute(),
      resolveInterpolableSVGLength(interpolableValue, lengthContext, m_unitMode,
                                   m_negativeValuesForbidden));
}

}  // namespace blink
