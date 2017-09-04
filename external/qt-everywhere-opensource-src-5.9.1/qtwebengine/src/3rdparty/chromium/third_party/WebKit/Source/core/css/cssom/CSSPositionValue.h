// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPositionValue_h
#define CSSPositionValue_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class CSSLengthValue;

class CORE_EXPORT CSSPositionValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSPositionValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSPositionValue* create(const CSSLengthValue* x,
                                  const CSSLengthValue* y) {
    return new CSSPositionValue(x, y);
  }

  // Bindings require a non const return value.
  CSSLengthValue* x() const { return const_cast<CSSLengthValue*>(m_x.get()); }
  CSSLengthValue* y() const { return const_cast<CSSLengthValue*>(m_y.get()); }

  StyleValueType type() const override { return PositionType; }

  CSSValue* toCSSValue() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_x);
    visitor->trace(m_y);
    CSSStyleValue::trace(visitor);
  }

 protected:
  CSSPositionValue(const CSSLengthValue* x, const CSSLengthValue* y)
      : m_x(x), m_y(y) {}

  Member<const CSSLengthValue> m_x;
  Member<const CSSLengthValue> m_y;
};

}  // namespace blink

#endif
