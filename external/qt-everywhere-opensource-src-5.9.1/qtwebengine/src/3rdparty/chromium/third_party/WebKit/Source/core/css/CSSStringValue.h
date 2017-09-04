// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStringValue_h
#define CSSStringValue_h

#include "core/css/CSSValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSStringValue : public CSSValue {
 public:
  static CSSStringValue* create(const String& str) {
    return new CSSStringValue(str);
  }

  String value() const { return m_string; }

  String customCSSText() const;

  bool equals(const CSSStringValue& other) const {
    return m_string == other.m_string;
  }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  CSSStringValue(const String&);

  String m_string;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSStringValue, isStringValue());

}  // namespace blink

#endif  // CSSStringValue_h
