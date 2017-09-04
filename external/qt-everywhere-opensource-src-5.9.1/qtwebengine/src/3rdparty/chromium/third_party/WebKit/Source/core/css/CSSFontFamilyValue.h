// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSFontFamilyValue_h
#define CSSFontFamilyValue_h

#include "core/css/CSSValue.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSFontFamilyValue : public CSSValue {
 public:
  static CSSFontFamilyValue* create(const String& familyName);

  String value() const { return m_string; }

  String customCSSText() const;

  bool equals(const CSSFontFamilyValue& other) const {
    return m_string == other.m_string;
  }

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  friend class CSSValuePool;

  CSSFontFamilyValue(const String&);

  // TODO(sashab): Change this to an AtomicString.
  String m_string;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSFontFamilyValue, isFontFamilyValue());

}  // namespace blink

#endif  // CSSFontFamilyValue_h
