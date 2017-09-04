// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleValueFactory.h"

#include "core/css/CSSImageValue.h"
#include "core/css/CSSValue.h"
#include "core/css/cssom/CSSNumberValue.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSStyleVariableReferenceValue.h"
#include "core/css/cssom/CSSTransformValue.h"
#include "core/css/cssom/CSSURLImageValue.h"
#include "core/css/cssom/CSSUnparsedValue.h"
#include "core/css/cssom/CSSUnsupportedStyleValue.h"

namespace blink {

namespace {

CSSStyleValue* styleValueForPrimitiveValue(
    const CSSPrimitiveValue& primitiveValue) {
  if (primitiveValue.isNumber())
    return CSSNumberValue::create(primitiveValue.getDoubleValue());
  if (primitiveValue.isLength() || primitiveValue.isPercentage())
    return CSSSimpleLength::fromCSSValue(primitiveValue);

  return nullptr;
}

CSSStyleValue* styleValueForProperty(CSSPropertyID propertyID,
                                     const CSSValue& value) {
  switch (propertyID) {
    case CSSPropertyTransform:
      return CSSTransformValue::fromCSSValue(value);
    default:
      // TODO(meade): Implement other complex properties.
      break;
  }

  if (value.isPrimitiveValue())
    return styleValueForPrimitiveValue(toCSSPrimitiveValue(value));
  if (value.isVariableReferenceValue())
    return CSSUnparsedValue::fromCSSValue(toCSSVariableReferenceValue(value));
  if (value.isImageValue())
    return CSSURLImageValue::create(
        toCSSImageValue(value).valueWithURLMadeAbsolute());

  return nullptr;
}

CSSStyleValueVector unsupportedCSSValue(const CSSValue& value) {
  CSSStyleValueVector styleValueVector;
  styleValueVector.append(CSSUnsupportedStyleValue::create(value.cssText()));
  return styleValueVector;
}

}  // namespace

CSSStyleValueVector StyleValueFactory::cssValueToStyleValueVector(
    CSSPropertyID propertyID,
    const CSSValue& value) {
  CSSStyleValueVector styleValueVector;
  CSSStyleValue* styleValue = styleValueForProperty(propertyID, value);
  if (styleValue) {
    styleValueVector.append(styleValue);
    return styleValueVector;
  }

  if (!value.isValueList()) {
    return unsupportedCSSValue(value);
  }

  // If it's a list, we can try it as a list valued property.
  const CSSValueList& cssValueList = toCSSValueList(value);
  for (const CSSValue* innerValue : cssValueList) {
    styleValue = styleValueForProperty(propertyID, *innerValue);
    if (!styleValue) {
      return unsupportedCSSValue(value);
    }
    styleValueVector.append(styleValue);
  }
  return styleValueVector;
}

}  // namespace blink
