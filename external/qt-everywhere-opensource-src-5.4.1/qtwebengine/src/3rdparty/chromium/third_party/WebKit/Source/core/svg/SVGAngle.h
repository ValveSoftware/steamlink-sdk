/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGAngle_h
#define SVGAngle_h

#include "core/svg/SVGEnumeration.h"

namespace WebCore {

class SVGAngle;
class SVGAngleTearOff;

enum SVGMarkerOrientType {
    SVGMarkerOrientUnknown = 0,
    SVGMarkerOrientAuto,
    SVGMarkerOrientAngle
};
template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGMarkerOrientType>();

class SVGMarkerOrientEnumeration : public SVGEnumeration<SVGMarkerOrientType> {
public:
    static PassRefPtr<SVGMarkerOrientEnumeration> create(SVGAngle* angle)
    {
        return adoptRef(new SVGMarkerOrientEnumeration(angle));
    }

    virtual ~SVGMarkerOrientEnumeration();

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float, unsigned, PassRefPtr<SVGPropertyBase>, PassRefPtr<SVGPropertyBase>, PassRefPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;

private:
    SVGMarkerOrientEnumeration(SVGAngle*);

    virtual void notifyChange() OVERRIDE;

    // FIXME: oilpan: This is kept as raw-ptr to avoid reference cycles. Should be Member in oilpan.
    SVGAngle* m_angle;
};

class SVGAngle : public SVGPropertyBase {
public:
    typedef SVGAngleTearOff TearOffType;

    enum SVGAngleType {
        SVG_ANGLETYPE_UNKNOWN = 0,
        SVG_ANGLETYPE_UNSPECIFIED = 1,
        SVG_ANGLETYPE_DEG = 2,
        SVG_ANGLETYPE_RAD = 3,
        SVG_ANGLETYPE_GRAD = 4,
        SVG_ANGLETYPE_TURN = 5
    };

    static PassRefPtr<SVGAngle> create()
    {
        return adoptRef(new SVGAngle());
    }

    virtual ~SVGAngle();

    SVGAngleType unitType() const { return m_unitType; }

    void setValue(float);
    float value() const;

    void setValueInSpecifiedUnits(float valueInSpecifiedUnits) { m_valueInSpecifiedUnits = valueInSpecifiedUnits; }
    float valueInSpecifiedUnits() const { return m_valueInSpecifiedUnits; }

    void newValueSpecifiedUnits(SVGAngleType unitType, float valueInSpecifiedUnits);
    void convertToSpecifiedUnits(SVGAngleType unitType, ExceptionState&);

    SVGEnumeration<SVGMarkerOrientType>* orientType() { return m_orientType.get(); }
    void orientTypeChanged();

    // SVGPropertyBase:

    PassRefPtr<SVGAngle> clone() const;
    virtual PassRefPtr<SVGPropertyBase> cloneForAnimation(const String&) const OVERRIDE;

    virtual String valueAsString() const OVERRIDE;
    void setValueAsString(const String&, ExceptionState&);

    virtual void add(PassRefPtrWillBeRawPtr<SVGPropertyBase>, SVGElement*) OVERRIDE;
    virtual void calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> from, PassRefPtr<SVGPropertyBase> to, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement* contextElement) OVERRIDE;
    virtual float calculateDistance(PassRefPtr<SVGPropertyBase> to, SVGElement* contextElement) OVERRIDE;

    static AnimatedPropertyType classType() { return AnimatedAngle; }

private:
    SVGAngle();
    SVGAngle(SVGAngleType, float, SVGMarkerOrientType);

    SVGAngleType m_unitType;
    float m_valueInSpecifiedUnits;
    RefPtr<SVGMarkerOrientEnumeration> m_orientType;
};

inline PassRefPtr<SVGAngle> toSVGAngle(PassRefPtr<SVGPropertyBase> passBase)
{
    RefPtr<SVGPropertyBase> base = passBase;
    ASSERT(base->type() == SVGAngle::classType());
    return static_pointer_cast<SVGAngle>(base.release());
}

} // namespace WebCore

#endif // SVGAngle_h
