// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSCalcLength.h"

#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/cssom/CSSCalcDictionary.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "wtf/Vector.h"

namespace blink {

CSSCalcLength::CSSCalcLength() : m_values(CSSLengthValue::kNumSupportedUnits), m_hasValues(CSSLengthValue::kNumSupportedUnits) {}

CSSCalcLength::CSSCalcLength(const CSSCalcLength& other) :
    m_values(other.m_values),
    m_hasValues(other.m_hasValues)
{}

CSSCalcLength::CSSCalcLength(const CSSSimpleLength& other) :
    m_values(CSSLengthValue::kNumSupportedUnits), m_hasValues(CSSLengthValue::kNumSupportedUnits)
{
    set(other.value(), other.lengthUnit());
}

CSSCalcLength* CSSCalcLength::create(const CSSLengthValue* length)
{
    if (length->type() == SimpleLengthType) {
        const CSSSimpleLength* simpleLength = toCSSSimpleLength(length);
        return new CSSCalcLength(*simpleLength);
    }

    return new CSSCalcLength(*toCSSCalcLength(length));
}

CSSCalcLength* CSSCalcLength::create(const CSSCalcDictionary& dictionary, ExceptionState& exceptionState)
{
    CSSCalcLength* result = new CSSCalcLength();
    int numSet = 0;

#define setFromDictValue(name, camelName, primitiveName) \
    if (dictionary.has##camelName()) { \
        result->set(dictionary.name(), CSSPrimitiveValue::UnitType::primitiveName); \
        numSet++; \
    }

    setFromDictValue(px, Px, Pixels)
    setFromDictValue(percent, Percent, Percentage)
    setFromDictValue(em, Em, Ems)
    setFromDictValue(ex, Ex, Exs)
    setFromDictValue(ch, Ch, Chs)
    setFromDictValue(rem, Rem, Rems)
    setFromDictValue(vw, Vw, ViewportWidth)
    setFromDictValue(vh, Vh, ViewportHeight)
    setFromDictValue(vmin, Vmin, ViewportMin)
    setFromDictValue(vmax, Vmax, ViewportMax)
    setFromDictValue(cm, Cm, Centimeters)
    setFromDictValue(mm, Mm, Millimeters)
    setFromDictValue(in, In, Inches)
    setFromDictValue(pc, Pc, Picas)
    setFromDictValue(pt, Pt, Points)

    if (numSet == 0) {
        exceptionState.throwTypeError("Must specify at least one value in CSSCalcDictionary for creating a CSSCalcLength.");
    }
    return result;
}

bool CSSCalcLength::containsPercent() const
{
    return has(CSSPrimitiveValue::UnitType::Percentage);
}

CSSLengthValue* CSSCalcLength::addInternal(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    CSSCalcLength* result = CSSCalcLength::create(other, exceptionState);
    for (int i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
        if (hasAtIndex(i)) {
            result->setAtIndex(getAtIndex(i) + result->getAtIndex(i), i);
        }
    }
    return result;
}

CSSLengthValue* CSSCalcLength::subtractInternal(const CSSLengthValue* other, ExceptionState& exceptionState)
{
    CSSCalcLength* result = CSSCalcLength::create(this, exceptionState);
    if (other->type() == CalcLengthType) {
        const CSSCalcLength* o = toCSSCalcLength(other);
        for (unsigned i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
            if (o->hasAtIndex(i)) {
                result->setAtIndex(getAtIndex(i) - o->getAtIndex(i), i);
            }
        }
    } else {
        const CSSSimpleLength* o = toCSSSimpleLength(other);
        result->set(get(o->lengthUnit()) - o->value(), o->lengthUnit());
    }
    return result;
}

CSSLengthValue* CSSCalcLength::multiplyInternal(double x, ExceptionState& exceptionState)
{
    CSSCalcLength* result = CSSCalcLength::create(this, exceptionState);
    for (unsigned i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
        if (hasAtIndex(i)) {
            result->setAtIndex(getAtIndex(i) * x, i);
        }
    }
    return result;
}

CSSLengthValue* CSSCalcLength::divideInternal(double x, ExceptionState& exceptionState)
{
    CSSCalcLength* result = CSSCalcLength::create(this, exceptionState);
    for (unsigned i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
        if (hasAtIndex(i)) {
            result->setAtIndex(getAtIndex(i) / x, i);
        }
    }
    return result;
}

CSSValue* CSSCalcLength::toCSSValue() const
{
    // Create a CSS Calc Value, then put it into a CSSPrimitiveValue
    CSSCalcExpressionNode* node = nullptr;
    for (unsigned i = 0; i < CSSLengthValue::kNumSupportedUnits; ++i) {
        if (!hasAtIndex(i))
            continue;
        double value = getAtIndex(i);
        if (node) {
            node = CSSCalcValue::createExpressionNode(
                node,
                CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(std::abs(value), unitFromIndex(i))),
                value >= 0 ? CalcAdd : CalcSubtract);
        } else {
            node = CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(value, unitFromIndex(i)));
        }
    }
    return CSSPrimitiveValue::create(CSSCalcValue::create(node));
}

int CSSCalcLength::indexForUnit(CSSPrimitiveValue::UnitType unit)
{
    return (static_cast<int>(unit) - static_cast<int>(CSSPrimitiveValue::UnitType::Percentage));
}

} // namespace blink
