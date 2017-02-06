// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSPerspective.h"

#include "bindings/core/v8/ExceptionState.h"

namespace blink {

CSSPerspective* CSSPerspective::create(const CSSLengthValue* length, ExceptionState& exceptionState)
{
    if (length->containsPercent()) {
        exceptionState.throwTypeError("CSSPerspective does not support CSSLengthValues with percent units");
        return nullptr;
    }
    return new CSSPerspective(length);
}

CSSFunctionValue* CSSPerspective::toCSSValue() const
{
    CSSFunctionValue* result = CSSFunctionValue::create(CSSValuePerspective);
    result->append(*m_length->toCSSValue());
    return result;
}

} // namespace blink
