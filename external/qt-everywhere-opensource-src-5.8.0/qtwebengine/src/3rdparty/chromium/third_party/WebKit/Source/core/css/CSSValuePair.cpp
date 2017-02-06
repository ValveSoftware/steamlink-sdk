// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSValuePair.h"

namespace blink {

DEFINE_TRACE_AFTER_DISPATCH(CSSValuePair)
{
    visitor->trace(m_first);
    visitor->trace(m_second);
    CSSValue::traceAfterDispatch(visitor);
}

}
