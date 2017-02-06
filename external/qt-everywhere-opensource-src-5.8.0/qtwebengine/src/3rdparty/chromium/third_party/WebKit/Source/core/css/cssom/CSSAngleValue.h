// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSAngleValue_h
#define CSSAngleValue_h

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSStyleValue.h"

namespace blink {

class CORE_EXPORT CSSAngleValue final : public CSSStyleValue {
    WTF_MAKE_NONCOPYABLE(CSSAngleValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSAngleValue* create(double value, const String& unit, ExceptionState&);

    StyleValueType type() const override { return AngleType; }

    double degrees() const;
    double radians() const;
    double gradians() const;
    double turns() const;

    CSSValue* toCSSValue() const override;
private:
    CSSAngleValue(double value, CSSPrimitiveValue::UnitType unit)
        : m_value(value)
        , m_unit(unit) {}

    double m_value;
    CSSPrimitiveValue::UnitType m_unit;
};

}

#endif // CSSAngleValue_h
