// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSAngleValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"
#include "wtf/MathExtras.h"

namespace blink {

CSSAngleValue* CSSAngleValue::create(double value, const String& unit, ExceptionState& exceptionState)
{
    CSSPrimitiveValue::UnitType primitiveUnit = CSSPrimitiveValue::stringToUnitType(unit);
    DCHECK(CSSPrimitiveValue::isAngle(primitiveUnit));
    return new CSSAngleValue(value, primitiveUnit);
}

double CSSAngleValue::degrees() const
{
    switch (m_unit) {
    case CSSPrimitiveValue::UnitType::Degrees:
        return m_value;
    case CSSPrimitiveValue::UnitType::Radians:
        return rad2deg(m_value);
    case CSSPrimitiveValue::UnitType::Gradians:
        return grad2deg(m_value);
    case CSSPrimitiveValue::UnitType::Turns:
        return turn2deg(m_value);
    default:
        NOTREACHED();
        return 0;
    }
}

double CSSAngleValue::radians() const
{
    return deg2rad(degrees());
}

double CSSAngleValue::gradians() const
{
    return deg2grad(degrees());
}

double CSSAngleValue::turns() const
{
    return deg2turn(degrees());
}

CSSValue* CSSAngleValue::toCSSValue() const
{
    return CSSPrimitiveValue::create(m_value, m_unit);
}

} // namespace blink
