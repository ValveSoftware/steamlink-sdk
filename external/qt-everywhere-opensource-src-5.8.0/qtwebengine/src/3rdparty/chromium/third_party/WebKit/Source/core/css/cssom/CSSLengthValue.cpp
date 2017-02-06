// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSLengthValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSCalcDictionary.h"
#include "core/css/cssom/CSSCalcLength.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "wtf/HashMap.h"

namespace blink {

CSSPrimitiveValue::UnitType CSSLengthValue::unitFromName(const String& name)
{
    if (equalIgnoringASCIICase(name, "percent") || name == "%")
        return CSSPrimitiveValue::UnitType::Percentage;
    return CSSPrimitiveValue::stringToUnitType(name);
}

CSSLengthValue* CSSLengthValue::from(const String& cssText, ExceptionState& exceptionState)
{
    // TODO: Implement
    return nullptr;
}

CSSLengthValue* CSSLengthValue::from(double value, const String& type, ExceptionState&)
{
    return CSSSimpleLength::create(value, unitFromName(type));
}

CSSLengthValue* CSSLengthValue::from(const CSSCalcDictionary& dictionary, ExceptionState& exceptionState)
{
    return CSSCalcLength::create(dictionary, exceptionState);
}

CSSLengthValue* CSSLengthValue::add(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    if (type() == other->type() || type() == CalcLengthType)
        return addInternal(other, exceptionState);

    CSSCalcLength* result = CSSCalcLength::create(this, exceptionState);
    return result->add(other, exceptionState);
}

CSSLengthValue* CSSLengthValue::subtract(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    if (type() == other->type() || type() == CalcLengthType)
        return subtractInternal(other, exceptionState);

    CSSCalcLength* result = CSSCalcLength::create(this, exceptionState);
    return result->subtract(other, exceptionState);
}

CSSLengthValue* CSSLengthValue::multiply(double x, ExceptionState& exceptionState)
{
    return multiplyInternal(x, exceptionState);
}

CSSLengthValue* CSSLengthValue::divide(double x, ExceptionState& exceptionState)
{
    return divideInternal(x, exceptionState);
}

CSSLengthValue* CSSLengthValue::addInternal(const CSSLengthValue*, ExceptionState&)
{
    NOTREACHED();
    return nullptr;
}

CSSLengthValue* CSSLengthValue::subtractInternal(const CSSLengthValue*, ExceptionState&)
{
    NOTREACHED();
    return nullptr;
}

CSSLengthValue* CSSLengthValue::multiplyInternal(double, ExceptionState&)
{
    NOTREACHED();
    return nullptr;
}

CSSLengthValue* CSSLengthValue::divideInternal(double, ExceptionState&)
{
    NOTREACHED();
    return nullptr;
}

} // namespace blink
