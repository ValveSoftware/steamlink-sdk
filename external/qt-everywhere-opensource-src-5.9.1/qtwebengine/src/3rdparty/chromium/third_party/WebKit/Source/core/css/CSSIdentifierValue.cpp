// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSIdentifierValue.h"

#include "core/css/CSSMarkup.h"
#include "core/css/CSSValuePool.h"
#include "platform/Length.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

static const AtomicString& valueName(CSSValueID valueID) {
  DCHECK_GE(valueID, 0);
  DCHECK_LT(valueID, numCSSValueKeywords);

  if (valueID < 0)
    return nullAtom;

  static AtomicString* keywordStrings =
      new AtomicString[numCSSValueKeywords];  // Leaked intentionally.
  AtomicString& keywordString = keywordStrings[valueID];
  if (keywordString.isNull())
    keywordString = getValueName(valueID);
  return keywordString;
}

CSSIdentifierValue* CSSIdentifierValue::create(CSSValueID valueID) {
  CSSIdentifierValue* cssValue = cssValuePool().identifierCacheValue(valueID);
  if (!cssValue) {
    cssValue = cssValuePool().setIdentifierCacheValue(
        valueID, new CSSIdentifierValue(valueID));
  }
  return cssValue;
}

String CSSIdentifierValue::customCSSText() const {
  return valueName(m_valueID);
}

CSSIdentifierValue::CSSIdentifierValue(CSSValueID valueID)
    : CSSValue(IdentifierClass), m_valueID(valueID) {
  // TODO(sashab): Add a DCHECK_NE(valueID, CSSValueInvalid) once no code paths
  // cause this to happen.
}

CSSIdentifierValue::CSSIdentifierValue(const Length& length)
    : CSSValue(IdentifierClass) {
  switch (length.type()) {
    case Auto:
      m_valueID = CSSValueAuto;
      break;
    case MinContent:
      m_valueID = CSSValueMinContent;
      break;
    case MaxContent:
      m_valueID = CSSValueMaxContent;
      break;
    case FillAvailable:
      m_valueID = CSSValueWebkitFillAvailable;
      break;
    case FitContent:
      m_valueID = CSSValueFitContent;
      break;
    case ExtendToZoom:
      m_valueID = CSSValueInternalExtendToZoom;
    case Percent:
    case Fixed:
    case Calculated:
    case DeviceWidth:
    case DeviceHeight:
    case MaxSizeNone:
      NOTREACHED();
      break;
  }
}

DEFINE_TRACE_AFTER_DISPATCH(CSSIdentifierValue) {
  CSSValue::traceAfterDispatch(visitor);
}

}  // namespace blink
