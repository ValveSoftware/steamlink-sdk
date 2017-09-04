// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSimpleLength.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSCalcLength.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSSimpleLength* CSSSimpleLength::create(double value,
                                         const String& type,
                                         ExceptionState& exceptionState) {
  CSSPrimitiveValue::UnitType unit = CSSLengthValue::unitFromName(type);
  if (!CSSLengthValue::isSupportedLengthUnit(unit)) {
    exceptionState.throwTypeError("Invalid unit for CSSSimpleLength: " + type);
    return nullptr;
  }
  return new CSSSimpleLength(value, unit);
}

CSSSimpleLength* CSSSimpleLength::fromCSSValue(const CSSPrimitiveValue& value) {
  DCHECK(value.isLength() || value.isPercentage());
  if (value.isPercentage())
    return new CSSSimpleLength(value.getDoubleValue(),
                               CSSPrimitiveValue::UnitType::Percentage);
  return new CSSSimpleLength(value.getDoubleValue(),
                             value.typeWithCalcResolved());
}

bool CSSSimpleLength::containsPercent() const {
  return lengthUnit() == CSSPrimitiveValue::UnitType::Percentage;
}

String CSSSimpleLength::unit() const {
  if (lengthUnit() == CSSPrimitiveValue::UnitType::Percentage)
    return "percent";
  return CSSPrimitiveValue::unitTypeToString(m_unit);
}

CSSLengthValue* CSSSimpleLength::addInternal(const CSSLengthValue* other) {
  const CSSSimpleLength* o = toCSSSimpleLength(other);
  DCHECK_EQ(m_unit, o->m_unit);
  return create(m_value + o->value(), m_unit);
}

CSSLengthValue* CSSSimpleLength::subtractInternal(const CSSLengthValue* other) {
  const CSSSimpleLength* o = toCSSSimpleLength(other);
  DCHECK_EQ(m_unit, o->m_unit);
  return create(m_value - o->value(), m_unit);
}

CSSLengthValue* CSSSimpleLength::multiplyInternal(double x) {
  return create(m_value * x, m_unit);
}

CSSLengthValue* CSSSimpleLength::divideInternal(double x) {
  DCHECK_NE(x, 0);
  return create(m_value / x, m_unit);
}

CSSValue* CSSSimpleLength::toCSSValue() const {
  return CSSPrimitiveValue::create(m_value, m_unit);
}

}  // namespace blink
