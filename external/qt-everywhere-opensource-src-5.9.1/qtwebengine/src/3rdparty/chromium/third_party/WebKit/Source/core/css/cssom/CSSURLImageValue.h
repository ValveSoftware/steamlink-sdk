// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSURLImageValue_h
#define CSSURLImageValue_h

#include "core/css/cssom/CSSStyleImageValue.h"

namespace blink {

class CORE_EXPORT CSSURLImageValue final : public CSSStyleImageValue {
  WTF_MAKE_NONCOPYABLE(CSSURLImageValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSURLImageValue* create(const AtomicString& url) {
    return new CSSURLImageValue(CSSImageValue::create(url));
  }
  static CSSURLImageValue* create(const CSSImageValue* imageValue) {
    return new CSSURLImageValue(imageValue);
  }

  StyleValueType type() const override { return URLImageType; }

  const CSSValue* toCSSValue() const override { return cssImageValue(); }

  const String& url() const { return cssImageValue()->url(); }

 private:
  explicit CSSURLImageValue(const CSSImageValue* imageValue)
      : CSSStyleImageValue(imageValue) {}
};

}  // namespace blink

#endif  // CSSResourceValue_h
