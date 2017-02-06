// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSURIValue.h"

#include "core/css/CSSMarkup.h"
#include "wtf/text/WTFString.h"

namespace blink {

CSSURIValue::CSSURIValue(const String& str)
    : CSSValue(URIClass)
    , m_string(str) { }

String CSSURIValue::customCSSText() const
{
    return serializeURI(m_string);
}

DEFINE_TRACE_AFTER_DISPATCH(CSSURIValue)
{
    CSSValue::traceAfterDispatch(visitor);
}

} // namespace blink
