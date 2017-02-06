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

#include "core/svg/SVGAngle.h"

#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "wtf/MathExtras.h"
#include "wtf/text/WTFString.h"

namespace blink {

template<> const SVGEnumerationStringEntries& getStaticStringEntries<SVGMarkerOrientType>()
{
    DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
    if (entries.isEmpty()) {
        entries.append(std::make_pair(SVGMarkerOrientAuto, "auto"));
        entries.append(std::make_pair(SVGMarkerOrientAngle, "angle"));
        entries.append(std::make_pair(SVGMarkerOrientAutoStartReverse, "auto-start-reverse"));
    }
    return entries;
}

template<> unsigned short getMaxExposedEnumValue<SVGMarkerOrientType>()
{
    return SVGMarkerOrientAngle;
}

SVGMarkerOrientEnumeration::SVGMarkerOrientEnumeration(SVGAngle* angle)
    : SVGEnumeration<SVGMarkerOrientType>(SVGMarkerOrientAngle)
    , m_angle(angle)
{
}

SVGMarkerOrientEnumeration::~SVGMarkerOrientEnumeration()
{
}

DEFINE_TRACE(SVGMarkerOrientEnumeration)
{
    visitor->trace(m_angle);
    SVGEnumeration<SVGMarkerOrientType>::trace(visitor);
}

void SVGMarkerOrientEnumeration::notifyChange()
{
    ASSERT(m_angle);
    m_angle->orientTypeChanged();
}

void SVGMarkerOrientEnumeration::add(SVGPropertyBase*, SVGElement*)
{
    // SVGMarkerOrientEnumeration is only animated via SVGAngle
    ASSERT_NOT_REACHED();
}

void SVGMarkerOrientEnumeration::calculateAnimatedValue(SVGAnimationElement*, float percentage, unsigned repeatCount, SVGPropertyBase* from, SVGPropertyBase* to, SVGPropertyBase* toAtEndOfDurationValue, SVGElement* contextElement)
{
    // SVGMarkerOrientEnumeration is only animated via SVGAngle
    ASSERT_NOT_REACHED();
}

float SVGMarkerOrientEnumeration::calculateDistance(SVGPropertyBase* to, SVGElement* contextElement)
{
    // SVGMarkerOrientEnumeration is only animated via SVGAngle
    ASSERT_NOT_REACHED();
    return -1.0;
}

SVGAngle::SVGAngle()
    : m_unitType(SVG_ANGLETYPE_UNSPECIFIED)
    , m_valueInSpecifiedUnits(0)
    , m_orientType(SVGMarkerOrientEnumeration::create(this))
{
}

SVGAngle::SVGAngle(SVGAngleType unitType, float valueInSpecifiedUnits, SVGMarkerOrientType orientType)
    : m_unitType(unitType)
    , m_valueInSpecifiedUnits(valueInSpecifiedUnits)
    , m_orientType(SVGMarkerOrientEnumeration::create(this))
{
    m_orientType->setEnumValue(orientType);
}

SVGAngle::~SVGAngle()
{
}

DEFINE_TRACE(SVGAngle)
{
    visitor->trace(m_orientType);
    SVGPropertyHelper<SVGAngle>::trace(visitor);
}

SVGAngle* SVGAngle::clone() const
{
    return new SVGAngle(m_unitType, m_valueInSpecifiedUnits, m_orientType->enumValue());
}

float SVGAngle::value() const
{
    switch (m_unitType) {
    case SVG_ANGLETYPE_GRAD:
        return grad2deg(m_valueInSpecifiedUnits);
    case SVG_ANGLETYPE_RAD:
        return rad2deg(m_valueInSpecifiedUnits);
    case SVG_ANGLETYPE_TURN:
        return turn2deg(m_valueInSpecifiedUnits);
    case SVG_ANGLETYPE_UNSPECIFIED:
    case SVG_ANGLETYPE_UNKNOWN:
    case SVG_ANGLETYPE_DEG:
        return m_valueInSpecifiedUnits;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

void SVGAngle::setValue(float value)
{
    switch (m_unitType) {
    case SVG_ANGLETYPE_GRAD:
        m_valueInSpecifiedUnits = deg2grad(value);
        break;
    case SVG_ANGLETYPE_RAD:
        m_valueInSpecifiedUnits = deg2rad(value);
        break;
    case SVG_ANGLETYPE_TURN:
        m_valueInSpecifiedUnits = deg2turn(value);
        break;
    case SVG_ANGLETYPE_UNSPECIFIED:
    case SVG_ANGLETYPE_UNKNOWN:
    case SVG_ANGLETYPE_DEG:
        m_valueInSpecifiedUnits = value;
        break;
    }
    m_orientType->setEnumValue(SVGMarkerOrientAngle);
}

template<typename CharType>
static SVGAngle::SVGAngleType stringToAngleType(const CharType*& ptr, const CharType* end)
{
    // If there's no unit given, the angle type is unspecified.
    if (ptr == end)
        return SVGAngle::SVG_ANGLETYPE_UNSPECIFIED;

    SVGAngle::SVGAngleType type = SVGAngle::SVG_ANGLETYPE_UNKNOWN;
    if (isHTMLSpace<CharType>(ptr[0])) {
        type = SVGAngle::SVG_ANGLETYPE_UNSPECIFIED;
        ptr++;
    } else if (end - ptr >= 3) {
        if (ptr[0] == 'd' && ptr[1] == 'e' && ptr[2] == 'g') {
            type = SVGAngle::SVG_ANGLETYPE_DEG;
            ptr += 3;
        } else if (ptr[0] == 'r' && ptr[1] == 'a' && ptr[2] == 'd') {
            type = SVGAngle::SVG_ANGLETYPE_RAD;
            ptr += 3;
        } else if (end - ptr >= 4) {
            if (ptr[0] == 'g' && ptr[1] == 'r' && ptr[2] == 'a' && ptr[3] == 'd') {
                type = SVGAngle::SVG_ANGLETYPE_GRAD;
                ptr += 4;
            } else if (ptr[0] == 't' && ptr[1] == 'u' && ptr[2] == 'r' && ptr[3] == 'n') {
                type = SVGAngle::SVG_ANGLETYPE_TURN;
                ptr += 4;
            }
        }
    }

    if (!skipOptionalSVGSpaces(ptr, end))
        return type;

    return SVGAngle::SVG_ANGLETYPE_UNKNOWN;
}

String SVGAngle::valueAsString() const
{
    switch (m_unitType) {
    case SVG_ANGLETYPE_DEG: {
        DEFINE_STATIC_LOCAL(String, degString, ("deg"));
        return String::number(m_valueInSpecifiedUnits) + degString;
    }
    case SVG_ANGLETYPE_RAD: {
        DEFINE_STATIC_LOCAL(String, radString, ("rad"));
        return String::number(m_valueInSpecifiedUnits) + radString;
    }
    case SVG_ANGLETYPE_GRAD: {
        DEFINE_STATIC_LOCAL(String, gradString, ("grad"));
        return String::number(m_valueInSpecifiedUnits) + gradString;
    }
    case SVG_ANGLETYPE_TURN: {
        DEFINE_STATIC_LOCAL(String, turnString, ("turn"));
        return String::number(m_valueInSpecifiedUnits) + turnString;
    }
    case SVG_ANGLETYPE_UNSPECIFIED:
    case SVG_ANGLETYPE_UNKNOWN:
        return String::number(m_valueInSpecifiedUnits);
    }

    ASSERT_NOT_REACHED();
    return String();
}

template<typename CharType>
static SVGParsingError parseValue(const String& value, float& valueInSpecifiedUnits, SVGAngle::SVGAngleType& unitType)
{
    const CharType* ptr = value.getCharacters<CharType>();
    const CharType* end = ptr + value.length();

    if (!parseNumber(ptr, end, valueInSpecifiedUnits, AllowLeadingWhitespace))
        return SVGParsingError(SVGParseStatus::ExpectedAngle, ptr - value.getCharacters<CharType>());

    unitType = stringToAngleType(ptr, end);
    if (unitType == SVGAngle::SVG_ANGLETYPE_UNKNOWN)
        return SVGParsingError(SVGParseStatus::ExpectedAngle, ptr - value.getCharacters<CharType>());

    return SVGParseStatus::NoError;
}

SVGParsingError SVGAngle::setValueAsString(const String& value)
{
    if (value.isEmpty()) {
        newValueSpecifiedUnits(SVG_ANGLETYPE_UNSPECIFIED, 0);
        return SVGParseStatus::NoError;
    }

    if (value == "auto") {
        newValueSpecifiedUnits(SVG_ANGLETYPE_UNSPECIFIED, 0);
        m_orientType->setEnumValue(SVGMarkerOrientAuto);
        return SVGParseStatus::NoError;
    }
    if (value == "auto-start-reverse") {
        newValueSpecifiedUnits(SVG_ANGLETYPE_UNSPECIFIED, 0);
        m_orientType->setEnumValue(SVGMarkerOrientAutoStartReverse);
        return SVGParseStatus::NoError;
    }

    float valueInSpecifiedUnits = 0;
    SVGAngleType unitType = SVG_ANGLETYPE_UNKNOWN;

    SVGParsingError error;
    if (value.is8Bit())
        error = parseValue<LChar>(value, valueInSpecifiedUnits, unitType);
    else
        error = parseValue<UChar>(value, valueInSpecifiedUnits, unitType);
    if (error != SVGParseStatus::NoError)
        return error;

    m_orientType->setEnumValue(SVGMarkerOrientAngle);
    m_unitType = unitType;
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
    return SVGParseStatus::NoError;
}

void SVGAngle::newValueSpecifiedUnits(SVGAngleType unitType, float valueInSpecifiedUnits)
{
    m_orientType->setEnumValue(SVGMarkerOrientAngle);
    m_unitType = unitType;
    m_valueInSpecifiedUnits = valueInSpecifiedUnits;
}

void SVGAngle::convertToSpecifiedUnits(SVGAngleType unitType)
{
    if (unitType == m_unitType)
        return;

    switch (m_unitType) {
    case SVG_ANGLETYPE_TURN:
        switch (unitType) {
        case SVG_ANGLETYPE_GRAD:
            m_valueInSpecifiedUnits = turn2grad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            m_valueInSpecifiedUnits = turn2deg(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_RAD:
            m_valueInSpecifiedUnits = deg2rad(turn2deg(m_valueInSpecifiedUnits));
            break;
        case SVG_ANGLETYPE_TURN:
        case SVG_ANGLETYPE_UNKNOWN:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case SVG_ANGLETYPE_RAD:
        switch (unitType) {
        case SVG_ANGLETYPE_GRAD:
            m_valueInSpecifiedUnits = rad2grad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            m_valueInSpecifiedUnits = rad2deg(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_TURN:
            m_valueInSpecifiedUnits = deg2turn(rad2deg(m_valueInSpecifiedUnits));
            break;
        case SVG_ANGLETYPE_RAD:
        case SVG_ANGLETYPE_UNKNOWN:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case SVG_ANGLETYPE_GRAD:
        switch (unitType) {
        case SVG_ANGLETYPE_RAD:
            m_valueInSpecifiedUnits = grad2rad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            m_valueInSpecifiedUnits = grad2deg(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_TURN:
            m_valueInSpecifiedUnits = grad2turn(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_GRAD:
        case SVG_ANGLETYPE_UNKNOWN:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case SVG_ANGLETYPE_UNSPECIFIED:
        // Spec: For angles, a unitless value is treated the same as if degrees were specified.
    case SVG_ANGLETYPE_DEG:
        switch (unitType) {
        case SVG_ANGLETYPE_RAD:
            m_valueInSpecifiedUnits = deg2rad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_GRAD:
            m_valueInSpecifiedUnits = deg2grad(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_TURN:
            m_valueInSpecifiedUnits = deg2turn(m_valueInSpecifiedUnits);
            break;
        case SVG_ANGLETYPE_UNSPECIFIED:
        case SVG_ANGLETYPE_DEG:
            break;
        case SVG_ANGLETYPE_UNKNOWN:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case SVG_ANGLETYPE_UNKNOWN:
        ASSERT_NOT_REACHED();
        break;
    }

    m_unitType = unitType;
    m_orientType->setEnumValue(SVGMarkerOrientAngle);
}

void SVGAngle::add(SVGPropertyBase* other, SVGElement*)
{
    SVGAngle* otherAngle = toSVGAngle(other);

    // Only respect by animations, if from and by are both specified in angles (and not eg. 'auto').
    if (orientType()->enumValue() != SVGMarkerOrientAngle || otherAngle->orientType()->enumValue() != SVGMarkerOrientAngle)
        return;

    setValue(value() + otherAngle->value());
}

void SVGAngle::assign(const SVGAngle& other)
{
    SVGMarkerOrientType otherOrientType = other.orientType()->enumValue();
    if (otherOrientType == SVGMarkerOrientAngle)
        newValueSpecifiedUnits(other.unitType(), other.valueInSpecifiedUnits());
    else
        m_orientType->setEnumValue(otherOrientType);
}

void SVGAngle::calculateAnimatedValue(SVGAnimationElement* animationElement, float percentage, unsigned repeatCount, SVGPropertyBase* from, SVGPropertyBase* to, SVGPropertyBase* toAtEndOfDuration, SVGElement*)
{
    ASSERT(animationElement);
    bool isToAnimation = animationElement->getAnimationMode() == ToAnimation;

    SVGAngle* fromAngle = isToAnimation ? this : toSVGAngle(from);
    SVGAngle* toAngle = toSVGAngle(to);
    SVGMarkerOrientType fromOrientType = fromAngle->orientType()->enumValue();
    SVGMarkerOrientType toOrientType = toAngle->orientType()->enumValue();

    if (fromOrientType != toOrientType) {
        // Fall back to discrete animation.
        assign(percentage < 0.5f ? *fromAngle : *toAngle);
        return;
    }

    switch (fromOrientType) {
    // From 'auto' to 'auto', or 'auto-start-reverse' to 'auto-start-reverse'
    case SVGMarkerOrientAuto:
    case SVGMarkerOrientAutoStartReverse:
        orientType()->setEnumValue(fromOrientType);
        return;

    // Regular from angle to angle animation, with all features like additive etc.
    case SVGMarkerOrientAngle:
        {
            float animatedValue = value();
            SVGAngle* toAtEndOfDurationAngle = toSVGAngle(toAtEndOfDuration);

            animationElement->animateAdditiveNumber(percentage, repeatCount, fromAngle->value(), toAngle->value(), toAtEndOfDurationAngle->value(), animatedValue);
            orientType()->setEnumValue(SVGMarkerOrientAngle);
            setValue(animatedValue);
        }
        return;

    // If the enumeration value is not angle or auto, its unknown.
    default:
        m_valueInSpecifiedUnits = 0;
        orientType()->setEnumValue(SVGMarkerOrientUnknown);
        return;
    }
}

float SVGAngle::calculateDistance(SVGPropertyBase* other, SVGElement*)
{
    return fabsf(value() - toSVGAngle(other)->value());
}

void SVGAngle::orientTypeChanged()
{
    if (orientType()->enumValue() == SVGMarkerOrientAuto || orientType()->enumValue() == SVGMarkerOrientAutoStartReverse) {
        m_unitType = SVG_ANGLETYPE_UNSPECIFIED;
        m_valueInSpecifiedUnits = 0;
    }
}

} // namespace blink
