// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSCalcLength_h
#define CSSCalcLength_h

#include "core/css/cssom/CSSLengthValue.h"
#include "wtf/BitVector.h"
#include <array>
#include <bitset>

namespace blink {

class CSSCalcExpressionNode;
class CSSSimpleLength;

class CORE_EXPORT CSSCalcLength final : public CSSLengthValue {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class UnitData {
   public:
    UnitData() : m_values(), m_hasValueForUnit() {}
    UnitData(const UnitData& other)
        : m_values(other.m_values),
          m_hasValueForUnit(other.m_hasValueForUnit) {}

    static std::unique_ptr<UnitData> fromExpressionNode(
        const CSSCalcExpressionNode*);
    CSSCalcExpressionNode* toCSSCalcExpressionNode() const;

    bool has(CSSPrimitiveValue::UnitType) const;
    void set(CSSPrimitiveValue::UnitType, double);
    double get(CSSPrimitiveValue::UnitType) const;

    void add(const UnitData& right);
    void subtract(const UnitData& right);
    void multiply(double);
    void divide(double);

   private:
    bool hasAtIndex(int) const;
    void setAtIndex(int, double);
    double getAtIndex(int) const;

    std::array<double, CSSLengthValue::kNumSupportedUnits> m_values;
    std::bitset<CSSLengthValue::kNumSupportedUnits> m_hasValueForUnit;
  };

  static CSSCalcLength* create(const CSSCalcDictionary&, ExceptionState&);
  static CSSCalcLength* create(const CSSLengthValue* length, ExceptionState&) {
    return create(length);
  }
  static CSSCalcLength* create(const CSSLengthValue*);
  static CSSCalcLength* fromCSSValue(const CSSPrimitiveValue&);

#define GETTER_MACRO(name, type)    \
  double name(bool& isNull) {       \
    isNull = !m_unitData.has(type); \
    return m_unitData.get(type);    \
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
  CSSLengthValue* addInternal(const CSSLengthValue* other) override;
  CSSLengthValue* subtractInternal(const CSSLengthValue* other) override;
  CSSLengthValue* multiplyInternal(double) override;
  CSSLengthValue* divideInternal(double) override;

 private:
  CSSCalcLength();
  CSSCalcLength(const CSSCalcLength& other);
  CSSCalcLength(const CSSSimpleLength& other);
  CSSCalcLength(const UnitData& unitData) : m_unitData(unitData) {}

  UnitData m_unitData;
};

#define DEFINE_CALC_LENGTH_TYPE_CASTS(argumentType)                    \
  DEFINE_TYPE_CASTS(                                                   \
      CSSCalcLength, argumentType, value,                              \
      value->type() == CSSLengthValue::StyleValueType::CalcLengthType, \
      value.type() == CSSLengthValue::StyleValueType::CalcLengthType)

DEFINE_CALC_LENGTH_TYPE_CASTS(CSSStyleValue);

}  // namespace blink

#endif
