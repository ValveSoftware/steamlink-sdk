// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSTranslation.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"

namespace blink {

CSSTranslation* CSSTranslation::create(CSSLengthValue* x, CSSLengthValue* y, CSSLengthValue* z, ExceptionState& exceptionState)
{
    if (z->containsPercent()) {
        exceptionState.throwTypeError("CSSTranslation does not support z CSSLengthValue with percent units");
        return nullptr;
    }
    return new CSSTranslation(x, y, z);
}

CSSFunctionValue* CSSTranslation::toCSSValue() const
{
    CSSFunctionValue* result = CSSFunctionValue::create(is2D() ? CSSValueTranslate : CSSValueTranslate3d);
    result->append(*m_x->toCSSValue());
    result->append(*m_y->toCSSValue());
    if (!is2D())
        result->append(*m_z->toCSSValue());
    return result;
}

} // namespace blink
