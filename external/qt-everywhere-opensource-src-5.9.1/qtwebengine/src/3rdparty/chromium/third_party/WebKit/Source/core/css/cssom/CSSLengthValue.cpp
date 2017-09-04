// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSLengthValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSCalcDictionary.h"
#include "core/css/cssom/CSSCalcLength.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "wtf/HashMap.h"

namespace blink {

CSSPrimitiveValue::UnitType CSSLengthValue::unitFromName(const String& name) {
  if (equalIgnoringASCIICase(name, "percent") || name == "%")
    return CSSPrimitiveValue::UnitType::Percentage;
  return CSSPrimitiveValue::stringToUnitType(name);
}

CSSLengthValue* CSSLengthValue::from(const String& cssText,
                                     ExceptionState& exceptionState) {
  // TODO: Implement
  return nullptr;
}

CSSLengthValue* CSSLengthValue::from(double value,
                                     const String& type,
                                     ExceptionState&) {
  return CSSSimpleLength::create(value, unitFromName(type));
}

CSSLengthValue* CSSLengthValue::from(const CSSCalcDictionary& dictionary,
                                     ExceptionState& exceptionState) {
  return CSSCalcLength::create(dictionary, exceptionState);
}

CSSLengthValue* CSSLengthValue::add(const CSSLengthValue* other) {
  if (type() == CalcLengthType)
    return addInternal(other);

  DCHECK_EQ(type(), SimpleLengthType);
  if (other->type() == SimpleLengthType &&
      toCSSSimpleLength(this)->unit() == toCSSSimpleLength(other)->unit()) {
    return addInternal(other);
  }

  // TODO(meade): This CalcLength is immediately thrown away. We might want
  // to optimize this at some point.
  CSSCalcLength* result = CSSCalcLength::create(this);
  return result->add(other);
}

CSSLengthValue* CSSLengthValue::subtract(const CSSLengthValue* other) {
  if (type() == CalcLengthType)
    return subtractInternal(other);

  DCHECK_EQ(type(), SimpleLengthType);
  if (other->type() == SimpleLengthType &&
      toCSSSimpleLength(this)->unit() == toCSSSimpleLength(other)->unit()) {
    return subtractInternal(other);
  }

  CSSCalcLength* result = CSSCalcLength::create(this);
  return result->subtract(other);
}

CSSLengthValue* CSSLengthValue::multiply(double x) {
  return multiplyInternal(x);
}

CSSLengthValue* CSSLengthValue::divide(double x,
                                       ExceptionState& exceptionState) {
  if (x == 0) {
    exceptionState.throwRangeError("Cannot divide by zero");
    return nullptr;
  }
  return divideInternal(x);
}

CSSLengthValue* CSSLengthValue::addInternal(const CSSLengthValue*) {
  NOTREACHED();
  return nullptr;
}

CSSLengthValue* CSSLengthValue::subtractInternal(const CSSLengthValue*) {
  NOTREACHED();
  return nullptr;
}

CSSLengthValue* CSSLengthValue::multiplyInternal(double) {
  NOTREACHED();
  return nullptr;
}

CSSLengthValue* CSSLengthValue::divideInternal(double) {
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
