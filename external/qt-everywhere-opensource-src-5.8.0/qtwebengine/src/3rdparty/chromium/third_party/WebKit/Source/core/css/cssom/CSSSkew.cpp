// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSkew.h"

#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSPrimitiveValue.h"

namespace blink {

CSSFunctionValue* CSSSkew::toCSSValue() const
{
    CSSFunctionValue* result = CSSFunctionValue::create(CSSValueSkew);
    result->append(*CSSPrimitiveValue::create(m_ax, CSSPrimitiveValue::UnitType::Number));
    result->append(*CSSPrimitiveValue::create(m_ay, CSSPrimitiveValue::UnitType::Number));
    return result;
}

} // namespace blink
