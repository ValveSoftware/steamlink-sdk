// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSCounterValue.h"

#include "core/css/CSSMarkup.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

String CSSCounterValue::customCSSText() const {
  StringBuilder result;
  if (separator().isEmpty())
    result.append("counter(");
  else
    result.append("counters(");

  result.append(identifier());
  if (!separator().isEmpty()) {
    result.append(", ");
    result.append(m_separator->cssText());
  }
  bool isDefaultListStyle = listStyle() == CSSValueDecimal;
  if (!isDefaultListStyle) {
    result.append(", ");
    result.append(m_listStyle->cssText());
  }
  result.append(')');

  return result.toString();
}

DEFINE_TRACE_AFTER_DISPATCH(CSSCounterValue) {
  visitor->trace(m_identifier);
  visitor->trace(m_listStyle);
  visitor->trace(m_separator);
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
