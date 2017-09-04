// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSSimpleLength_h
#define CSSSimpleLength_h

#include "core/css/cssom/CSSLengthValue.h"

namespace blink {

class CSSPrimitiveValue;
class ExceptionState;

class CORE_EXPORT CSSSimpleLength final : public CSSLengthValue {
  WTF_MAKE_NONCOPYABLE(CSSSimpleLength);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSSimpleLength* create(double, const String& type, ExceptionState&);
  static CSSSimpleLength* create(double value,
                                 CSSPrimitiveValue::UnitType type) {
    return new CSSSimpleLength(value, type);
  }
  static CSSSimpleLength* fromCSSValue(const CSSPrimitiveValue&);

  bool containsPercent() const override;

  double value() const { return m_value; }
  String unit() const;
  CSSPrimitiveValue::UnitType lengthUnit() const { return m_unit; }

  StyleValueType type() const override {
    return StyleValueType::SimpleLengthType;
  }

  CSSValue* toCSSValue() const override;

 protected:
  virtual CSSLengthValue* addInternal(const CSSLengthValue* other);
  virtual CSSLengthValue* subtractInternal(const CSSLengthValue* other);
  virtual CSSLengthValue* multiplyInternal(double);
  virtual CSSLengthValue* divideInternal(double);

 private:
  CSSSimpleLength(double value, CSSPrimitiveValue::UnitType unit)
      : CSSLengthValue(), m_unit(unit), m_value(value) {}

  CSSPrimitiveValue::UnitType m_unit;
  double m_value;
};

#define DEFINE_SIMPLE_LENGTH_TYPE_CASTS(argumentType)                    \
  DEFINE_TYPE_CASTS(                                                     \
      CSSSimpleLength, argumentType, value,                              \
      value->type() == CSSLengthValue::StyleValueType::SimpleLengthType, \
      value.type() == CSSLengthValue::StyleValueType::SimpleLengthType)

DEFINE_SIMPLE_LENGTH_TYPE_CASTS(CSSLengthValue);
DEFINE_SIMPLE_LENGTH_TYPE_CASTS(CSSStyleValue);

}  // namespace blink

#endif
