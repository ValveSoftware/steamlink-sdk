// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSCustomIdentValue.h"

#include "core/css/CSSMarkup.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSSCustomIdentValue::CSSCustomIdentValue(const AtomicString& str)
    : CSSValue(CustomIdentClass),
      m_string(str),
      m_propertyId(CSSPropertyInvalid) {}

CSSCustomIdentValue::CSSCustomIdentValue(CSSPropertyID id)
    : CSSValue(CustomIdentClass), m_string(), m_propertyId(id) {
  ASSERT(isKnownPropertyID());
}

String CSSCustomIdentValue::customCSSText() const {
  if (isKnownPropertyID())
    return getPropertyNameAtomicString(m_propertyId);
  StringBuilder builder;
  serializeIdentifier(m_string, builder);
  return builder.toString();
}

DEFINE_TRACE_AFTER_DISPATCH(CSSCustomIdentValue) {
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
