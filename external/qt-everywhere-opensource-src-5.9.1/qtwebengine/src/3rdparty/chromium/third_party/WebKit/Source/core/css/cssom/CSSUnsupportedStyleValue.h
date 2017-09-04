// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSUnsupportedStyleValue_h
#define CSSUnsupportedStyleValue_h

#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class CORE_EXPORT CSSUnsupportedStyleValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSUnsupportedStyleValue);

 public:
  static CSSUnsupportedStyleValue* create(const String& cssText) {
    return new CSSUnsupportedStyleValue(cssText);
  }

  StyleValueType type() const override { return StyleValueType::Unknown; }
  const CSSValue* toCSSValue() const override;
  const CSSValue* toCSSValueWithProperty(CSSPropertyID) const override;
  String cssText() const override { return m_cssText; }

 private:
  CSSUnsupportedStyleValue(const String& cssText) : m_cssText(cssText) {}

  String m_cssText;
};

}  // namespace blink

#endif  // CSSUnsupportedStyleValue_h
