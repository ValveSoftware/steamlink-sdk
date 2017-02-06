/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGLength_h
#define SVGLength_h

#include "core/css/CSSPrimitiveValue.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/SVGProperty.h"
#include "platform/heap/Handle.h"

namespace blink {

class QualifiedName;

class SVGLengthTearOff;

class SVGLength final : public SVGPropertyBase {
public:
    typedef SVGLengthTearOff TearOffType;

    static SVGLength* create(SVGLengthMode mode = SVGLengthMode::Other)
    {
        return new SVGLength(mode);
    }

    DECLARE_VIRTUAL_TRACE();

    SVGLength* clone() const;
    SVGPropertyBase* cloneForAnimation(const String&) const override;

    CSSPrimitiveValue::UnitType typeWithCalcResolved() const { return m_value->typeWithCalcResolved(); }
    void setUnitType(CSSPrimitiveValue::UnitType);
    SVGLengthMode unitMode() const { return static_cast<SVGLengthMode>(m_unitMode); }

    bool operator==(const SVGLength&) const;
    bool operator!=(const SVGLength& other) const { return !operator==(other); }

    float value(const SVGLengthContext&) const;
    void setValue(float, const SVGLengthContext&);

    float valueInSpecifiedUnits() const { return m_value->getFloatValue(); }
    void setValueInSpecifiedUnits(float value)
    {
        m_value = CSSPrimitiveValue::create(value, m_value->typeWithCalcResolved());
    }

    CSSPrimitiveValue* asCSSPrimitiveValue() const { return m_value.get(); }

    // Resolves LengthTypePercentage into a normalized floating point number (full value is 1.0).
    float valueAsPercentage() const;

    // Returns a number to be used as percentage (so full value is 100)
    float valueAsPercentage100() const;

    // Scale the input value by this SVGLength. Higher precision than input * valueAsPercentage().
    float scaleByPercentage(float) const;

    String valueAsString() const override;
    SVGParsingError setValueAsString(const String&);

    void newValueSpecifiedUnits(CSSPrimitiveValue::UnitType, float valueInSpecifiedUnits);
    void convertToSpecifiedUnits(CSSPrimitiveValue::UnitType, const SVGLengthContext&);

    // Helper functions
    inline bool isRelative() const { return CSSPrimitiveValue::isRelativeUnit(m_value->typeWithCalcResolved()); }

    bool isZero() const
    {
        return m_value->getFloatValue() == 0;
    }

    static SVGLengthMode lengthModeForAnimatedLengthAttribute(const QualifiedName&);
    static bool negativeValuesForbiddenForAnimatedLengthAttribute(const QualifiedName&);

    void add(SVGPropertyBase*, SVGElement*) override;
    void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, SVGPropertyBase* from, SVGPropertyBase* to, SVGPropertyBase* toAtEndOfDurationValue, SVGElement* contextElement) override;
    float calculateDistance(SVGPropertyBase* to, SVGElement* contextElement) override;

    static AnimatedPropertyType classType() { return AnimatedLength; }
    AnimatedPropertyType type() const override { return classType(); }

private:
    SVGLength(SVGLengthMode);
    SVGLength(const SVGLength&);

    Member<CSSPrimitiveValue> m_value;
    unsigned m_unitMode : 2;
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGLength);

} // namespace blink

#endif // SVGLength_h
