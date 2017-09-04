// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSNumberValue_h
#define CSSNumberValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT CSSNumberValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSNumberValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSNumberValue* create(double value) {
    return new CSSNumberValue(value);
  }

  double value() const { return m_value; }

  CSSValue* toCSSValue() const override {
    return CSSPrimitiveValue::create(m_value,
                                     CSSPrimitiveValue::UnitType::Number);
  }

  StyleValueType type() const override { return StyleValueType::NumberType; }

 private:
  CSSNumberValue(double value) : m_value(value) {}

  double m_value;
};

}  // namespace blink

#endif
