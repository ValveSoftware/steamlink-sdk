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
#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/SVGPropertyHelper.h"
#include "platform/heap/Handle.h"

namespace blink {

class SVGAngle;
class SVGAngleTearOff;

enum SVGMarkerOrientType {
  SVGMarkerOrientUnknown = 0,
  SVGMarkerOrientAuto,
  SVGMarkerOrientAngle,
  SVGMarkerOrientAutoStartReverse
};
template <>
const SVGEnumerationStringEntries&
getStaticStringEntries<SVGMarkerOrientType>();
template <>
unsigned short getMaxExposedEnumValue<SVGMarkerOrientType>();

class SVGMarkerOrientEnumeration final
    : public SVGEnumeration<SVGMarkerOrientType> {
 public:
  static SVGMarkerOrientEnumeration* create(SVGAngle* angle) {
    return new SVGMarkerOrientEnumeration(angle);
  }

  ~SVGMarkerOrientEnumeration() override;

  void add(SVGPropertyBase*, SVGElement*) override;
  void calculateAnimatedValue(SVGAnimationElement*,
                              float,
                              unsigned,
                              SVGPropertyBase*,
                              SVGPropertyBase*,
                              SVGPropertyBase*,
                              SVGElement*) override;
  float calculateDistance(SVGPropertyBase*, SVGElement*) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  SVGMarkerOrientEnumeration(SVGAngle*);

  void notifyChange() override;

  Member<SVGAngle> m_angle;
};

class SVGAngle final : public SVGPropertyHelper<SVGAngle> {
 public:
  typedef SVGAngleTearOff TearOffType;

  enum SVGAngleType {
    kSvgAngletypeUnknown = 0,
    kSvgAngletypeUnspecified = 1,
    kSvgAngletypeDeg = 2,
    kSvgAngletypeRad = 3,
    kSvgAngletypeGrad = 4,
    kSvgAngletypeTurn = 5
  };

  static SVGAngle* create() { return new SVGAngle(); }

  ~SVGAngle() override;

  SVGAngleType unitType() const { return m_unitType; }

  void setValue(float);
  float value() const;

  void setValueInSpecifiedUnits(float valueInSpecifiedUnits) {
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
  }
  float valueInSpecifiedUnits() const { return m_valueInSpecifiedUnits; }

  void newValueSpecifiedUnits(SVGAngleType unitType,
                              float valueInSpecifiedUnits);
  void convertToSpecifiedUnits(SVGAngleType unitType);

  SVGEnumeration<SVGMarkerOrientType>* orientType() {
    return m_orientType.get();
  }
  const SVGEnumeration<SVGMarkerOrientType>* orientType() const {
    return m_orientType.get();
  }
  void orientTypeChanged();

  // SVGPropertyBase:

  SVGAngle* clone() const;

  String valueAsString() const override;
  SVGParsingError setValueAsString(const String&);

  void add(SVGPropertyBase*, SVGElement*) override;
  void calculateAnimatedValue(SVGAnimationElement*,
                              float percentage,
                              unsigned repeatCount,
                              SVGPropertyBase* from,
                              SVGPropertyBase* to,
                              SVGPropertyBase* toAtEndOfDurationValue,
                              SVGElement* contextElement) override;
  float calculateDistance(SVGPropertyBase* to,
                          SVGElement* contextElement) override;

  static AnimatedPropertyType classType() { return AnimatedAngle; }

  DECLARE_VIRTUAL_TRACE();

 private:
  SVGAngle();
  SVGAngle(SVGAngleType, float, SVGMarkerOrientType);

  void assign(const SVGAngle&);

  SVGAngleType m_unitType;
  float m_valueInSpecifiedUnits;
  Member<SVGMarkerOrientEnumeration> m_orientType;
};

DEFINE_SVG_PROPERTY_TYPE_CASTS(SVGAngle);

}  // namespace blink

#endif  // SVGAngle_h
