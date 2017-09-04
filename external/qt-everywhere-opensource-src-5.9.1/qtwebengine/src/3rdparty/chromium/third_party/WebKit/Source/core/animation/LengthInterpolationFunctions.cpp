// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/LengthInterpolationFunctions.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "platform/CalculationValue.h"

namespace blink {

// This class is implemented as a singleton whose instance represents the
// presence of percentages being used in a Length value while nullptr represents
// the absence of any percentages.
class CSSLengthNonInterpolableValue : public NonInterpolableValue {
 public:
  ~CSSLengthNonInterpolableValue() final { NOTREACHED(); }
  static PassRefPtr<CSSLengthNonInterpolableValue> create(bool hasPercentage) {
    DEFINE_STATIC_REF(CSSLengthNonInterpolableValue, singleton,
                      adoptRef(new CSSLengthNonInterpolableValue()));
    DCHECK(singleton);
    return hasPercentage ? singleton : nullptr;
  }
  static PassRefPtr<CSSLengthNonInterpolableValue> merge(
      const NonInterpolableValue* a,
      const NonInterpolableValue* b) {
    return create(hasPercentage(a) || hasPercentage(b));
  }
  static bool hasPercentage(const NonInterpolableValue*);

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSLengthNonInterpolableValue() {}
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSLengthNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSLengthNonInterpolableValue);

bool CSSLengthNonInterpolableValue::hasPercentage(
    const NonInterpolableValue* nonInterpolableValue) {
  DCHECK(isCSSLengthNonInterpolableValue(nonInterpolableValue));
  return static_cast<bool>(nonInterpolableValue);
}

std::unique_ptr<InterpolableValue>
LengthInterpolationFunctions::createInterpolablePixels(double pixels) {
  std::unique_ptr<InterpolableList> interpolableList =
      createNeutralInterpolableValue();
  interpolableList->set(CSSPrimitiveValue::UnitTypePixels,
                        InterpolableNumber::create(pixels));
  return std::move(interpolableList);
}

InterpolationValue LengthInterpolationFunctions::createInterpolablePercent(
    double percent) {
  std::unique_ptr<InterpolableList> interpolableList =
      createNeutralInterpolableValue();
  interpolableList->set(CSSPrimitiveValue::UnitTypePercentage,
                        InterpolableNumber::create(percent));
  return InterpolationValue(std::move(interpolableList),
                            CSSLengthNonInterpolableValue::create(true));
}

std::unique_ptr<InterpolableList>
LengthInterpolationFunctions::createNeutralInterpolableValue() {
  const size_t length = CSSPrimitiveValue::LengthUnitTypeCount;
  std::unique_ptr<InterpolableList> values = InterpolableList::create(length);
  for (size_t i = 0; i < length; i++)
    values->set(i, InterpolableNumber::create(0));
  return values;
}

InterpolationValue LengthInterpolationFunctions::maybeConvertCSSValue(
    const CSSValue& value) {
  if (!value.isPrimitiveValue())
    return nullptr;

  const CSSPrimitiveValue& primitiveValue = toCSSPrimitiveValue(value);
  if (!primitiveValue.isLength() && !primitiveValue.isPercentage() &&
      !primitiveValue.isCalculatedPercentageWithLength())
    return nullptr;

  CSSLengthArray lengthArray;
  primitiveValue.accumulateLengthArray(lengthArray);

  std::unique_ptr<InterpolableList> values =
      InterpolableList::create(CSSPrimitiveValue::LengthUnitTypeCount);
  for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++)
    values->set(i, InterpolableNumber::create(lengthArray.values[i]));

  bool hasPercentage =
      lengthArray.typeFlags.get(CSSPrimitiveValue::UnitTypePercentage);
  return InterpolationValue(
      std::move(values), CSSLengthNonInterpolableValue::create(hasPercentage));
}

InterpolationValue LengthInterpolationFunctions::maybeConvertLength(
    const Length& length,
    float zoom) {
  if (!length.isSpecified())
    return nullptr;

  PixelsAndPercent pixelsAndPercent = length.getPixelsAndPercent();
  std::unique_ptr<InterpolableList> values = createNeutralInterpolableValue();
  values->set(CSSPrimitiveValue::UnitTypePixels,
              InterpolableNumber::create(pixelsAndPercent.pixels / zoom));
  values->set(CSSPrimitiveValue::UnitTypePercentage,
              InterpolableNumber::create(pixelsAndPercent.percent));

  return InterpolationValue(
      std::move(values),
      CSSLengthNonInterpolableValue::create(length.isPercentOrCalc()));
}

PairwiseInterpolationValue LengthInterpolationFunctions::mergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) {
  return PairwiseInterpolationValue(
      std::move(start.interpolableValue), std::move(end.interpolableValue),
      CSSLengthNonInterpolableValue::merge(start.nonInterpolableValue.get(),
                                           end.nonInterpolableValue.get()));
}

bool LengthInterpolationFunctions::nonInterpolableValuesAreCompatible(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  DCHECK(isCSSLengthNonInterpolableValue(a));
  DCHECK(isCSSLengthNonInterpolableValue(b));
  return true;
}

void LengthInterpolationFunctions::composite(
    std::unique_ptr<InterpolableValue>& underlyingInterpolableValue,
    RefPtr<NonInterpolableValue>& underlyingNonInterpolableValue,
    double underlyingFraction,
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue) {
  underlyingInterpolableValue->scaleAndAdd(underlyingFraction,
                                           interpolableValue);
  underlyingNonInterpolableValue = CSSLengthNonInterpolableValue::merge(
      underlyingNonInterpolableValue.get(), nonInterpolableValue);
}

void LengthInterpolationFunctions::subtractFromOneHundredPercent(
    InterpolationValue& result) {
  InterpolableList& list = toInterpolableList(*result.interpolableValue);
  for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
    double value = -toInterpolableNumber(*list.get(i)).value();
    if (i == CSSPrimitiveValue::UnitTypePercentage)
      value += 100;
    toInterpolableNumber(*list.getMutable(i)).set(value);
  }
  result.nonInterpolableValue = CSSLengthNonInterpolableValue::create(true);
}

static double clampToRange(double x, ValueRange range) {
  return (range == ValueRangeNonNegative && x < 0) ? 0 : x;
}

Length LengthInterpolationFunctions::createLength(
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue,
    const CSSToLengthConversionData& conversionData,
    ValueRange range) {
  const InterpolableList& interpolableList =
      toInterpolableList(interpolableValue);
  bool hasPercentage =
      CSSLengthNonInterpolableValue::hasPercentage(nonInterpolableValue);
  double pixels = 0;
  double percentage = 0;
  for (size_t i = 0; i < CSSPrimitiveValue::LengthUnitTypeCount; i++) {
    double value = toInterpolableNumber(*interpolableList.get(i)).value();
    if (value == 0)
      continue;
    if (i == CSSPrimitiveValue::UnitTypePercentage) {
      percentage = value;
    } else {
      CSSPrimitiveValue::UnitType type =
          CSSPrimitiveValue::lengthUnitTypeToUnitType(
              static_cast<CSSPrimitiveValue::LengthUnitType>(i));
      pixels += conversionData.zoomedComputedPixels(value, type);
    }
  }

  if (percentage != 0)
    hasPercentage = true;
  if (pixels != 0 && hasPercentage)
    return Length(
        CalculationValue::create(PixelsAndPercent(pixels, percentage), range));
  if (hasPercentage)
    return Length(clampToRange(percentage, range), Percent);
  return Length(
      CSSPrimitiveValue::clampToCSSLengthRange(clampToRange(pixels, range)),
      Fixed);
}

}  // namespace blink
