// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSSimpleLength.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSCalcLength.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSValue* CSSSimpleLength::toCSSValue() const
{
    return CSSPrimitiveValue::create(m_value, m_unit);
}

bool CSSSimpleLength::containsPercent() const
{
    return lengthUnit() == CSSPrimitiveValue::UnitType::Percentage;
}

CSSLengthValue* CSSSimpleLength::addInternal(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    const CSSSimpleLength* o = toCSSSimpleLength(other);
    if (m_unit == o->m_unit)
        return create(m_value + o->value(), m_unit);

    // Different units resolve to a calc.
    CSSCalcLength* result = CSSCalcLength::create(this, exceptionState);
    return result->add(other, exceptionState);
}

CSSLengthValue* CSSSimpleLength::subtractInternal(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    const CSSSimpleLength* o = toCSSSimpleLength(other);
    if (m_unit == o->m_unit)
        return create(m_value - o->value(), m_unit);

    // Different units resolve to a calc.
    CSSCalcLength* result = CSSCalcLength::create(this, exceptionState);
    return result->subtract(other, exceptionState);
}

CSSLengthValue* CSSSimpleLength::multiplyInternal(double x, ExceptionState& exceptionState)
{
    return create(m_value * x, m_unit);
}

CSSLengthValue* CSSSimpleLength::divideInternal(double x, ExceptionState& exceptionState)
{
    return create(m_value / x, m_unit);
}

} // namespace blink
