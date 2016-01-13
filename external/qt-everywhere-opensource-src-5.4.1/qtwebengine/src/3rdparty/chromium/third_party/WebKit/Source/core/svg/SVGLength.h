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

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/properties/SVGProperty.h"
#include "platform/heap/Handle.h"

namespace WebCore {

class CSSPrimitiveValue;
class ExceptionState;
class QualifiedName;

enum SVGLengthNegativeValuesMode {
    AllowNegativeLengths,
    ForbidNegativeLengths
};

class SVGLengthTearOff;

class SVGLength FINAL : public SVGPropertyBase {
public:
    typedef SVGLengthTearOff TearOffType;

    static PassRefPtr<SVGLength> create(SVGLengthMode mode = LengthModeOther)
    {
        return adoptRef(new SVGLength(mode));
    }

    PassRefPtr<SVGLength> clone() const;
    virtual PassRefPtr<SVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;

    SVGLengthType unitType() const { return static_cast<SVGLengthType>(m_unitType); }
    void setUnitType(SVGLengthType);
    SVGLengthMode unitMode() const { return static_cast<SVGLengthMode>(m_unitMode); }

    bool operator==(const SVGLength&) const;
    bool operator!=(const SVGLength& other) const { return !operator==(other); }

    float value(const SVGLengthContext& context) const
    {
        return value(context, IGNORE_EXCEPTION);
    }
    float value(const SVGLengthContext&, ExceptionState&) const;
    void setValue(float, const SVGLengthContext&, ExceptionState&);

    float valueInSpecifiedUnits() const { return m_valueInSpecifiedUnits; }
    void setValueInSpecifiedUnits(float value) { m_valueInSpecifiedUnits = value; }

    float valueAsPercentage() const;

    virtual String valueAsString() const OVERRIDE;
    void setValueAsString(const String&, ExceptionState&);

    void newValueSpecifiedUnits(SVGLengthType, float valueInSpecifiedUnits);
    void convertToSpecifiedUnits(SVGLengthType, const SVGLengthContext&, ExceptionState&);

    // Helper functions
    inline bool isRelative() const
    {
        return m_unitType == LengthTypePercentage
            || m_unitType == LengthTypeEMS
            || m_unitType == LengthTypeEXS;
    }

    bool isZero() const
    {
        return !m_valueInSpecifiedUnits;
    }

    static PassRefPtr<SVGLength> fromCSSPrimitiveValue(CSSPrimitiveValue*);
    static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> toCSSPrimitiveValue(PassRefPtr<SVGLength>);
    static SVGLengthMode lengthModeForAnimatedLengthAttribute(const QualifiedName&);

    PassRefPtr<SVGLength> blend(PassRefPtr<SVGLength> from, float progress) const;

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> from, PassRefPtr<SVGPropertyBase> to, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement* contextElement) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement* contextElement) OVERRIDE;

    static AnimatedPropertyType classType() { return AnimatedLength; }

private:
    SVGLength(SVGLengthMode);
    SVGLength(const SVGLength&);

    float m_valueInSpecifiedUnits;
    unsigned m_unitMode : 2;
    unsigned m_unitType : 4;
};

inline PassRefPtr<SVGLength> toSVGLength(PassRefPtr<SVGPropertyBase> passBase)
{
    RefPtr<SVGPropertyBase> base = passBase;
    ASSERT(base->type() == SVGLength::classType());
    return static_pointer_cast<SVGLength>(base.release());
}

} // namespace WebCore

#endif // SVGLength_h
