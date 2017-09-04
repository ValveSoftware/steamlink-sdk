// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSFontFamilyValue.h"

#include "core/css/CSSMarkup.h"
#include "core/css/CSSValuePool.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSSFontFamilyValue* CSSFontFamilyValue::create(const String& familyName) {
  if (familyName.isNull())
    return new CSSFontFamilyValue(familyName);
  CSSValuePool::FontFamilyValueCache::AddResult entry =
      cssValuePool().getFontFamilyCacheEntry(familyName);
  if (!entry.storedValue->value)
    entry.storedValue->value = new CSSFontFamilyValue(familyName);
  return entry.storedValue->value;
}

CSSFontFamilyValue::CSSFontFamilyValue(const String& str)
    : CSSValue(FontFamilyClass), m_string(str) {}

String CSSFontFamilyValue::customCSSText() const {
  return serializeFontFamily(m_string);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSFontFamilyValue) {
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
