// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/SizeInterpolationFunctions.h"

#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/UnderlyingValueOwner.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/CSSValuePair.h"

namespace blink {

class CSSSizeNonInterpolableValue : public NonInterpolableValue {
 public:
  static PassRefPtr<CSSSizeNonInterpolableValue> create(CSSValueID keyword) {
    return adoptRef(new CSSSizeNonInterpolableValue(keyword));
  }

  static PassRefPtr<CSSSizeNonInterpolableValue> create(
      PassRefPtr<NonInterpolableValue> lengthNonInterpolableValue) {
    return adoptRef(
        new CSSSizeNonInterpolableValue(std::move(lengthNonInterpolableValue)));
  }

  bool isKeyword() const { return m_keyword != CSSValueInvalid; }
  CSSValueID keyword() const {
    DCHECK(isKeyword());
    return m_keyword;
  }

  const NonInterpolableValue* lengthNonInterpolableValue() const {
    DCHECK(!isKeyword());
    return m_lengthNonInterpolableValue.get();
  }
  RefPtr<NonInterpolableValue>& lengthNonInterpolableValue() {
    DCHECK(!isKeyword());
    return m_lengthNonInterpolableValue;
  }

  DECLARE_NON_INTERPOLABLE_VALUE_TYPE();

 private:
  CSSSizeNonInterpolableValue(CSSValueID keyword)
      : m_keyword(keyword), m_lengthNonInterpolableValue(nullptr) {
    DCHECK_NE(keyword, CSSValueInvalid);
  }

  CSSSizeNonInterpolableValue(
      PassRefPtr<NonInterpolableValue> lengthNonInterpolableValue)
      : m_keyword(CSSValueInvalid),
        m_lengthNonInterpolableValue(lengthNonInterpolableValue) {}

  CSSValueID m_keyword;
  RefPtr<NonInterpolableValue> m_lengthNonInterpolableValue;
};

DEFINE_NON_INTERPOLABLE_VALUE_TYPE(CSSSizeNonInterpolableValue);
DEFINE_NON_INTERPOLABLE_VALUE_TYPE_CASTS(CSSSizeNonInterpolableValue);

static InterpolationValue convertKeyword(CSSValueID keyword) {
  return InterpolationValue(InterpolableList::create(0),
                            CSSSizeNonInterpolableValue::create(keyword));
}

static InterpolationValue wrapConvertedLength(
    InterpolationValue&& convertedLength) {
  if (!convertedLength)
    return nullptr;
  return InterpolationValue(
      std::move(convertedLength.interpolableValue),
      CSSSizeNonInterpolableValue::create(
          convertedLength.nonInterpolableValue.release()));
}

InterpolationValue SizeInterpolationFunctions::convertFillSizeSide(
    const FillSize& fillSize,
    float zoom,
    bool convertWidth) {
  switch (fillSize.type) {
    case SizeLength: {
      const Length& side =
          convertWidth ? fillSize.size.width() : fillSize.size.height();
      if (side.isAuto())
        return convertKeyword(CSSValueAuto);
      return wrapConvertedLength(
          LengthInterpolationFunctions::maybeConvertLength(side, zoom));
    }
    case Contain:
      return convertKeyword(CSSValueContain);
    case Cover:
      return convertKeyword(CSSValueCover);
    case SizeNone:
    default:
      NOTREACHED();
      return nullptr;
  }
}

InterpolationValue SizeInterpolationFunctions::maybeConvertCSSSizeSide(
    const CSSValue& value,
    bool convertWidth) {
  if (value.isValuePair()) {
    const CSSValuePair& pair = toCSSValuePair(value);
    const CSSValue& side = convertWidth ? pair.first() : pair.second();
    if (side.isIdentifierValue() &&
        toCSSIdentifierValue(side).getValueID() == CSSValueAuto)
      return convertKeyword(CSSValueAuto);
    return wrapConvertedLength(
        LengthInterpolationFunctions::maybeConvertCSSValue(side));
  }

  if (!value.isIdentifierValue() && !value.isPrimitiveValue())
    return nullptr;
  if (value.isIdentifierValue())
    return convertKeyword(toCSSIdentifierValue(value).getValueID());

  // A single length is equivalent to "<length> auto".
  if (convertWidth)
    return wrapConvertedLength(
        LengthInterpolationFunctions::maybeConvertCSSValue(value));
  return convertKeyword(CSSValueAuto);
}

PairwiseInterpolationValue SizeInterpolationFunctions::maybeMergeSingles(
    InterpolationValue&& start,
    InterpolationValue&& end) {
  if (!nonInterpolableValuesAreCompatible(start.nonInterpolableValue.get(),
                                          end.nonInterpolableValue.get()))
    return nullptr;
  return PairwiseInterpolationValue(std::move(start.interpolableValue),
                                    std::move(end.interpolableValue),
                                    start.nonInterpolableValue.release());
}

InterpolationValue SizeInterpolationFunctions::createNeutralValue(
    const NonInterpolableValue* nonInterpolableValue) {
  auto& size = toCSSSizeNonInterpolableValue(*nonInterpolableValue);
  if (size.isKeyword())
    return convertKeyword(size.keyword());
  return wrapConvertedLength(InterpolationValue(
      LengthInterpolationFunctions::createNeutralInterpolableValue()));
}

bool SizeInterpolationFunctions::nonInterpolableValuesAreCompatible(
    const NonInterpolableValue* a,
    const NonInterpolableValue* b) {
  const auto& sizeA = toCSSSizeNonInterpolableValue(*a);
  const auto& sizeB = toCSSSizeNonInterpolableValue(*b);
  if (sizeA.isKeyword() != sizeB.isKeyword())
    return false;
  if (sizeA.isKeyword())
    return sizeA.keyword() == sizeB.keyword();
  return true;
}

void SizeInterpolationFunctions::composite(
    std::unique_ptr<InterpolableValue>& underlyingInterpolableValue,
    RefPtr<NonInterpolableValue>& underlyingNonInterpolableValue,
    double underlyingFraction,
    const InterpolableValue& interpolableValue,
    const NonInterpolableValue* nonInterpolableValue) {
  const auto& sizeNonInterpolableValue =
      toCSSSizeNonInterpolableValue(*nonInterpolableValue);
  if (sizeNonInterpolableValue.isKeyword())
    return;
  auto& underlyingSizeNonInterpolableValue =
      toCSSSizeNonInterpolableValue(*underlyingNonInterpolableValue);
  LengthInterpolationFunctions::composite(
      underlyingInterpolableValue,
      underlyingSizeNonInterpolableValue.lengthNonInterpolableValue(),
      underlyingFraction, interpolableValue,
      sizeNonInterpolableValue.lengthNonInterpolableValue());
}

static Length createLength(
    const InterpolableValue& interpolableValue,
    const CSSSizeNonInterpolableValue& nonInterpolableValue,
    const CSSToLengthConversionData& conversionData) {
  if (nonInterpolableValue.isKeyword()) {
    DCHECK_EQ(nonInterpolableValue.keyword(), CSSValueAuto);
    return Length(Auto);
  }
  return LengthInterpolationFunctions::createLength(
      interpolableValue, nonInterpolableValue.lengthNonInterpolableValue(),
      conversionData, ValueRangeNonNegative);
}

FillSize SizeInterpolationFunctions::createFillSize(
    const InterpolableValue& interpolableValueA,
    const NonInterpolableValue* nonInterpolableValueA,
    const InterpolableValue& interpolableValueB,
    const NonInterpolableValue* nonInterpolableValueB,
    const CSSToLengthConversionData& conversionData) {
  const auto& sideA = toCSSSizeNonInterpolableValue(*nonInterpolableValueA);
  const auto& sideB = toCSSSizeNonInterpolableValue(*nonInterpolableValueB);
  if (sideA.isKeyword()) {
    switch (sideA.keyword()) {
      case CSSValueCover:
        DCHECK_EQ(sideA.keyword(), sideB.keyword());
        return FillSize(Cover, LengthSize());
      case CSSValueContain:
        DCHECK_EQ(sideA.keyword(), sideB.keyword());
        return FillSize(Contain, LengthSize());
      case CSSValueAuto:
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return FillSize(
      SizeLength,
      LengthSize(createLength(interpolableValueA, sideA, conversionData),
                 createLength(interpolableValueB, sideB, conversionData)));
}

}  // namespace blink
