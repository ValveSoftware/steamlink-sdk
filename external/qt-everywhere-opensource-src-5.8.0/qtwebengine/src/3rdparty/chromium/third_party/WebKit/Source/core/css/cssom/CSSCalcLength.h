// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSCalcLength_h
#define CSSCalcLength_h

#include "core/css/cssom/CSSLengthValue.h"
#include "wtf/BitVector.h"

namespace blink {

class CSSCalcDictionary;
class CSSSimpleLength;

class CORE_EXPORT CSSCalcLength final : public CSSLengthValue {
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSCalcLength* create(const CSSLengthValue*);
    static CSSCalcLength* create(const CSSCalcDictionary&, ExceptionState&);
    static CSSCalcLength* create(const CSSLengthValue* length, ExceptionState&)
    {
        return create(length);
    }

#define GETTER_MACRO(name, type) \
    double name(bool& isNull) \
    { \
        isNull = !hasAtIndex(indexForUnit(type)); \
        return get(type); \
    }

    GETTER_MACRO(px, CSSPrimitiveValue::UnitType::Pixels)
    GETTER_MACRO(percent, CSSPrimitiveValue::UnitType::Percentage)
    GETTER_MACRO(em, CSSPrimitiveValue::UnitType::Ems)
    GETTER_MACRO(ex, CSSPrimitiveValue::UnitType::Exs)
    GETTER_MACRO(ch, CSSPrimitiveValue::UnitType::Chs)
    GETTER_MACRO(rem, CSSPrimitiveValue::UnitType::Rems)
    GETTER_MACRO(vw, CSSPrimitiveValue::UnitType::ViewportWidth)
    GETTER_MACRO(vh, CSSPrimitiveValue::UnitType::ViewportHeight)
    GETTER_MACRO(vmin, CSSPrimitiveValue::UnitType::ViewportMin)
    GETTER_MACRO(vmax, CSSPrimitiveValue::UnitType::ViewportMax)
    GETTER_MACRO(cm, CSSPrimitiveValue::UnitType::Centimeters)
    GETTER_MACRO(mm, CSSPrimitiveValue::UnitType::Millimeters)
    GETTER_MACRO(in, CSSPrimitiveValue::UnitType::Inches)
    GETTER_MACRO(pc, CSSPrimitiveValue::UnitType::Picas)
    GETTER_MACRO(pt, CSSPrimitiveValue::UnitType::Points)

#undef GETTER_MACRO

    bool containsPercent() const override;

    CSSValue* toCSSValue() const override;

    StyleValueType type() const override { return CalcLengthType; }
protected:
    CSSLengthValue* addInternal(const CSSLengthValue* other, ExceptionState&) override;
    CSSLengthValue* subtractInternal(const CSSLengthValue* other, ExceptionState&) override;
    CSSLengthValue* multiplyInternal(double, ExceptionState&) override;
    CSSLengthValue* divideInternal(double, ExceptionState&) override;

private:
    CSSCalcLength();
    CSSCalcLength(const CSSCalcLength& other);
    CSSCalcLength(const CSSSimpleLength& other);

    static int indexForUnit(CSSPrimitiveValue::UnitType);
    static CSSPrimitiveValue::UnitType unitFromIndex(int index)
    {
        DCHECK(index < CSSLengthValue::kNumSupportedUnits);
        int lowestValue = static_cast<int>(CSSPrimitiveValue::UnitType::Percentage);
        return static_cast<CSSPrimitiveValue::UnitType>(index + lowestValue);
    }

    bool hasAtIndex(int index) const
    {
        DCHECK(index < CSSLengthValue::kNumSupportedUnits);
        return m_hasValues.quickGet(index);
    }
    bool has(CSSPrimitiveValue::UnitType unit) const { return hasAtIndex(indexForUnit(unit)); }
    void setAtIndex(double value, int index)
    {
        DCHECK(index < CSSLengthValue::kNumSupportedUnits);
        m_hasValues.quickSet(index);
        m_values[index] = value;
    }
    void set(double value, CSSPrimitiveValue::UnitType unit) { setAtIndex(value, indexForUnit(unit)); }
    double getAtIndex(int index) const
    {
        DCHECK(index < CSSLengthValue::kNumSupportedUnits);
        return m_values[index];
    }
    double get(CSSPrimitiveValue::UnitType unit) const { return getAtIndex(indexForUnit(unit)); }

    Vector<double, CSSLengthValue::kNumSupportedUnits> m_values;
    BitVector m_hasValues;
};

#define DEFINE_CALC_LENGTH_TYPE_CASTS(argumentType) \
    DEFINE_TYPE_CASTS(CSSCalcLength, argumentType, value, \
        value->type() == CSSLengthValue::StyleValueType::CalcLengthType, \
        value.type() == CSSLengthValue::StyleValueType::CalcLengthType)

DEFINE_CALC_LENGTH_TYPE_CASTS(CSSStyleValue);

} // namespace blink

#endif
