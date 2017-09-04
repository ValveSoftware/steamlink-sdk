// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/CSSPositionAxisListInterpolationType.h"

#include "core/animation/LengthInterpolationFunctions.h"
#include "core/animation/ListInterpolationFunctions.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"

namespace blink {

InterpolationValue
CSSPositionAxisListInterpolationType::convertPositionAxisCSSValue(
    const CSSValue& value) {
  if (value.isValuePair()) {
    const CSSValuePair& pair = toCSSValuePair(value);
    InterpolationValue result =
        LengthInterpolationFunctions::maybeConvertCSSValue(pair.second());
    CSSValueID side = toCSSIdentifierValue(pair.first()).getValueID();
    if (side == CSSValueRight || side == CSSValueBottom)
      LengthInterpolationFunctions::subtractFromOneHundredPercent(result);
    return result;
  }

  if (value.isPrimitiveValue())
    return LengthInterpolationFunctions::maybeConvertCSSValue(value);

  if (!value.isIdentifierValue())
    return nullptr;

  const CSSIdentifierValue& ident = toCSSIdentifierValue(value);
  switch (ident.getValueID()) {
    case CSSValueLeft:
    case CSSValueTop:
      return LengthInterpolationFunctions::createInterpolablePercent(0);
    case CSSValueRight:
    case CSSValueBottom:
      return LengthInterpolationFunctions::createInterpolablePercent(100);
    case CSSValueCenter:
      return LengthInterpolationFunctions::createInterpolablePercent(50);
    default:
      NOTREACHED();
      return nullptr;
  }
}

InterpolationValue CSSPositionAxisListInterpolationType::maybeConvertValue(
    const CSSValue& value,
    const StyleResolverState&,
    ConversionCheckers&) const {
  if (!value.isBaseValueList()) {
    return ListInterpolationFunctions::createList(
        1, [&value](size_t) { return convertPositionAxisCSSValue(value); });
  }

  const CSSValueList& list = toCSSValueList(value);
  return ListInterpolationFunctions::createList(
      list.length(), [&list](size_t index) {
        return convertPositionAxisCSSValue(list.item(index));
      });
}

}  // namespace blink
