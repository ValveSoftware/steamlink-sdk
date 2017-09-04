// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSStringValue.h"

#include "core/css/CSSMarkup.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSSStringValue::CSSStringValue(const String& str)
    : CSSValue(StringClass), m_string(str) {}

String CSSStringValue::customCSSText() const {
  return serializeString(m_string);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSStringValue) {
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
