// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSPendingSubstitutionValue.h"

namespace blink {

DEFINE_TRACE_AFTER_DISPATCH(CSSPendingSubstitutionValue)
{
    CSSValue::traceAfterDispatch(visitor);
    visitor->trace(m_shorthandValue);
}

String CSSPendingSubstitutionValue::customCSSText() const
{
    return "";
}

} // namespace blink
