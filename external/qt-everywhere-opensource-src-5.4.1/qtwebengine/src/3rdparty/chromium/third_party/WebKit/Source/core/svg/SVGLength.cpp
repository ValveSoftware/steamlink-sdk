/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "config.h"

#include "core/svg/SVGLength.h"

#include "bindings/v8/ExceptionState.h"
#include "core/SVGNames.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/ExceptionCode.h"
#include "core/svg/SVGAnimationElement.h"
#include "core/svg/SVGParserUtilities.h"
#include "platform/animation/AnimationUtilities.h"
#include "wtf/MathExtras.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

namespace {

inline String lengthTypeToString(SVGLengthType type)
{
    switch (type) {
    case LengthTypeUnknown:
    case LengthTypeNumber:
        return "";
    case LengthTypePercentage:
        return "%";
    case LengthTypeEMS:
        return "em";
    case LengthTypeEXS:
        return "ex";
    case LengthTypePX:
        return "px";
    case LengthTypeCM:
        return "cm";
    case LengthTypeMM:
        return "mm";
    case LengthTypeIN:
        return "in";
    case LengthTypePT:
        return "pt";
    case LengthTypePC:
        return "pc";
    }

    ASSERT_NOT_REACHED();
    return String();
}

template<typename CharType>
SVGLengthType stringToLengthType(const CharType*& ptr, const CharType* end)
{
    if (ptr == end)
        return LengthTypeNumber;

    SVGLengthType type = LengthTypeUnknown;
    const CharType firstChar = *ptr++;

    if (firstChar == '%') {
        type = LengthTypePercentage;
    } else if (isHTMLSpace<CharType>(firstChar)) {
        type = LengthTypeNumber;
    } else if (ptr < end) {
        const CharType secondChar = *ptr++;

        if (firstChar == 'p') {
            if (secondChar == 'x')
                type = LengthTypePX;
            if (secondChar == 't')
                type = LengthTypePT;
            if (secondChar == 'c')
                type = LengthTypePC;
        } else if (firstChar == 'e') {
            if (secondChar == 'm')
                type = LengthTypeEMS;
            if (secondChar == 'x')
                type = LengthTypeEXS;
        } else if (firstChar == 'c' && secondChar == 'm') {
            type = LengthTypeCM;
        } else if (firstChar == 'm' && secondChar == 'm') {
            type = LengthTypeMM;
        } else if (firstChar == 'i' && secondChar == 'n') {
            type = LengthTypeIN;
        } else if (isHTMLSpace<CharType>(firstChar) && isHTMLSpace<CharType>(secondChar)) {
            type = LengthTypeNumber;
        }
    }

    if (!skipOptionalSVGSpaces(ptr, end))
        return type;

    return LengthTypeUnknown;
}

} // namespace

SVGLength::SVGLength(SVGLengthMode mode)
    : SVGPropertyBase(classType())
    , m_valueInSpecifiedUnits(0)
    , m_unitMode(mode)
    , m_unitType(LengthTypeNumber)
{
}

SVGLength::SVGLength(const SVGLength& o)
    : SVGPropertyBase(classType())
    , m_valueInSpecifiedUnits(o.m_valueInSpecifiedUnits)
    , m_unitMode(o.m_unitMode)
    , m_unitType(o.m_unitType)
{
}

PassRefPtr<SVGLength> SVGLength::clone() const
{
    return adoptRef(new SVGLength(*this));
}

PassRefPtr<SVGPropertyBase> SVGLength::cloneForAnimation(const String& value) const
{
    RefPtr<SVGLength> length = create();

    length->m_unitMode = m_unitMode;
    length->m_unitType = m_unitType;

    TrackExceptionState exceptionState;
    length->setValueAsString(value, exceptionState);
    if (exceptionState.hadException()) {
        length->m_unitType = LengthTypeNumber;
        length->m_valueInSpecifiedUnits = 0;
    }

    return length.release();
}

bool SVGLength::operator==(const SVGLength& other) const
{
    return m_unitMode == other.m_unitMode
        && m_unitType == other.m_unitType
        && m_valueInSpecifiedUnits == other.m_valueInSpecifiedUnits;
}

float SVGLength::value(const SVGLengthContext& context, ExceptionState& es) const
{
    return context.convertValueToUserUnits(m_valueInSpecifiedUnits, unitMode(), unitType(), es);
}

void SVGLength::setValue(float value, const SVGLengthContext& context, ExceptionState& es)
{
    // 100% = 100.0 instead of 1.0 for historical reasons, this could eventually be changed
    if (m_unitType == LengthTypePercentage)
        value = value / 100;

    float convertedValue = context.convertValueFromUserUnits(value, unitMode(), unitType(), es);
    if (es.hadException())
        return;

    m_valueInSpecifiedUnits = convertedValue;
}

void SVGLength::setUnitType(SVGLengthType type)
{
    ASSERT(type != LengthTypeUnknown && type <= LengthTypePC);
    m_unitType = type;
}

float SVGLength::valueAsPercentage() const
{
    // 100% = 100.0 instead of 1.0 for historical reasons, this could eventually be changed
    if (m_unitType == LengthTypePercentage)
        return m_valueInSpecifiedUnits / 100;

    return m_valueInSpecifiedUnits;
}

template<typename CharType>
static bool parseValueInternal(const String& string, float& convertedNumber, SVGLengthType& type)
{
    const CharType* ptr = string.getCharacters<CharType>();
    const CharType* end = ptr + string.length();

    if (!parseNumber(ptr, end, convertedNumber, AllowLeadingWhitespace))
        return false;

    type = stringToLengthType(ptr, end);
    ASSERT(ptr <= end);
    if (type == LengthTypeUnknown)
        return false;

    return true;
}

void SVGLength::setValueAsString(const String& string, ExceptionState& exceptionState)
{
    if (string.isEmpty()) {
        m_unitType = LengthTypeNumber;
        m_valueInSpecifiedUnits = 0;
        return;
    }

    float convertedNumber = 0;
    SVGLengthType type = LengthTypeUnknown;

    bool success = string.is8Bit() ?
        parseValueInternal<LChar>(string, convertedNumber, type) :
        parseValueInternal<UChar>(string, convertedNumber, type);

    if (!success) {
        exceptionState.throwDOMException(SyntaxError, "The value provided ('" + string + "') is invalid.");
        return;
    }

    m_unitType = type;
    m_valueInSpecifiedUnits = convertedNumber;
}

String SVGLength::valueAsString() const
{
    return String::number(m_valueInSpecifiedUnits) + lengthTypeToString(unitType());
}

void SVGLength::newValueSpecifiedUnits(SVGLengthType type, float value)
{
    setUnitType(type);
    m_valueInSpecifiedUnits = value;
}

void SVGLength::convertToSpecifiedUnits(SVGLengthType type, const SVGLengthContext& context, ExceptionState& exceptionState)
{
    ASSERT(type != LengthTypeUnknown && type <= LengthTypePC);

    float valueInUserUnits = value(context, exceptionState);
    if (exceptionState.hadException())
        return;

    SVGLengthType originalType = unitType();
    m_unitType = type;
    setValue(valueInUserUnits, context, exceptionState);
    if (!exceptionState.hadException())
        return;

    // Eventually restore old unit and type
    m_unitType = originalType;
}

PassRefPtr<SVGLength> SVGLength::fromCSSPrimitiveValue(CSSPrimitiveValue* value)
{
    ASSERT(value);

    SVGLengthType svgType;
    switch (value->primitiveType()) {
    case CSSPrimitiveValue::CSS_NUMBER:
        svgType = LengthTypeNumber;
        break;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        svgType = LengthTypePercentage;
        break;
    case CSSPrimitiveValue::CSS_EMS:
        svgType = LengthTypeEMS;
        break;
    case CSSPrimitiveValue::CSS_EXS:
        svgType = LengthTypeEXS;
        break;
    case CSSPrimitiveValue::CSS_PX:
        svgType = LengthTypePX;
        break;
    case CSSPrimitiveValue::CSS_CM:
        svgType = LengthTypeCM;
        break;
    case CSSPrimitiveValue::CSS_MM:
        svgType = LengthTypeMM;
        break;
    case CSSPrimitiveValue::CSS_IN:
        svgType = LengthTypeIN;
        break;
    case CSSPrimitiveValue::CSS_PT:
        svgType = LengthTypePT;
        break;
    case CSSPrimitiveValue::CSS_PC:
        svgType = LengthTypePC;
        break;
    case CSSPrimitiveValue::CSS_UNKNOWN:
    default:
        svgType = LengthTypeUnknown;
        break;
    };

    if (svgType == LengthTypeUnknown)
        return SVGLength::create();

    RefPtr<SVGLength> length = SVGLength::create();
    length->newValueSpecifiedUnits(svgType, value->getFloatValue());
    return length.release();
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> SVGLength::toCSSPrimitiveValue(PassRefPtr<SVGLength> passLength)
{
    RefPtr<SVGLength> length = passLength;

    CSSPrimitiveValue::UnitType cssType = CSSPrimitiveValue::CSS_UNKNOWN;
    switch (length->unitType()) {
    case LengthTypeUnknown:
        break;
    case LengthTypeNumber:
        cssType = CSSPrimitiveValue::CSS_NUMBER;
        break;
    case LengthTypePercentage:
        cssType = CSSPrimitiveValue::CSS_PERCENTAGE;
        break;
    case LengthTypeEMS:
        cssType = CSSPrimitiveValue::CSS_EMS;
        break;
    case LengthTypeEXS:
        cssType = CSSPrimitiveValue::CSS_EXS;
        break;
    case LengthTypePX:
        cssType = CSSPrimitiveValue::CSS_PX;
        break;
    case LengthTypeCM:
        cssType = CSSPrimitiveValue::CSS_CM;
        break;
    case LengthTypeMM:
        cssType = CSSPrimitiveValue::CSS_MM;
        break;
    case LengthTypeIN:
        cssType = CSSPrimitiveValue::CSS_IN;
        break;
    case LengthTypePT:
        cssType = CSSPrimitiveValue::CSS_PT;
        break;
    case LengthTypePC:
        cssType = CSSPrimitiveValue::CSS_PC;
        break;
    };

    return CSSPrimitiveValue::create(length->valueInSpecifiedUnits(), cssType);
}

SVGLengthMode SVGLength::lengthModeForAnimatedLengthAttribute(const QualifiedName& attrName)
{
    typedef HashMap<QualifiedName, SVGLengthMode> LengthModeForLengthAttributeMap;
    DEFINE_STATIC_LOCAL(LengthModeForLengthAttributeMap, s_lengthModeMap, ());

    if (s_lengthModeMap.isEmpty()) {
        s_lengthModeMap.set(SVGNames::xAttr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::yAttr, LengthModeHeight);
        s_lengthModeMap.set(SVGNames::cxAttr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::cyAttr, LengthModeHeight);
        s_lengthModeMap.set(SVGNames::dxAttr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::dyAttr, LengthModeHeight);
        s_lengthModeMap.set(SVGNames::fxAttr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::fyAttr, LengthModeHeight);
        s_lengthModeMap.set(SVGNames::rAttr, LengthModeOther);
        s_lengthModeMap.set(SVGNames::rxAttr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::ryAttr, LengthModeHeight);
        s_lengthModeMap.set(SVGNames::widthAttr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::heightAttr, LengthModeHeight);
        s_lengthModeMap.set(SVGNames::x1Attr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::x2Attr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::y1Attr, LengthModeHeight);
        s_lengthModeMap.set(SVGNames::y2Attr, LengthModeHeight);
        s_lengthModeMap.set(SVGNames::refXAttr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::refYAttr, LengthModeHeight);
        s_lengthModeMap.set(SVGNames::markerWidthAttr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::markerHeightAttr, LengthModeHeight);
        s_lengthModeMap.set(SVGNames::textLengthAttr, LengthModeWidth);
        s_lengthModeMap.set(SVGNames::startOffsetAttr, LengthModeWidth);
    }

    if (s_lengthModeMap.contains(attrName))
        return s_lengthModeMap.get(attrName);

    return LengthModeOther;
}

PassRefPtr<SVGLength> SVGLength::blend(PassRefPtr<SVGLength> passFrom, float progress) const
{
    RefPtr<SVGLength> from = passFrom;

    SVGLengthType toType = unitType();
    SVGLengthType fromType = from->unitType();
    if ((from->isZero() && isZero())
        || fromType == LengthTypeUnknown
        || toType == LengthTypeUnknown
        || (!from->isZero() && fromType != LengthTypePercentage && toType == LengthTypePercentage)
        || (!isZero() && fromType == LengthTypePercentage && toType != LengthTypePercentage)
        || (!from->isZero() && !isZero() && (fromType == LengthTypeEMS || fromType == LengthTypeEXS) && fromType != toType))
        return clone();

    RefPtr<SVGLength> length = create();

    if (fromType == LengthTypePercentage || toType == LengthTypePercentage) {
        float fromPercent = from->valueAsPercentage() * 100;
        float toPercent = valueAsPercentage() * 100;
        length->newValueSpecifiedUnits(LengthTypePercentage, WebCore::blend(fromPercent, toPercent, progress));
        return length;
    }

    if (fromType == toType || from->isZero() || isZero() || fromType == LengthTypeEMS || fromType == LengthTypeEXS) {
        float fromValue = from->valueInSpecifiedUnits();
        float toValue = valueInSpecifiedUnits();
        if (isZero())
            length->newValueSpecifiedUnits(fromType, WebCore::blend(fromValue, toValue, progress));
        else
            length->newValueSpecifiedUnits(toType, WebCore::blend(fromValue, toValue, progress));
        return length;
    }

    ASSERT(!isRelative());
    ASSERT(!from->isRelative());

    TrackExceptionState es;
    SVGLengthContext nonRelativeLengthContext(0);
    float fromValueInUserUnits = nonRelativeLengthContext.convertValueToUserUnits(from->valueInSpecifiedUnits(), from->unitMode(), fromType, es);
    if (es.hadException())
        return create();

    float fromValue = nonRelativeLengthContext.convertValueFromUserUnits(fromValueInUserUnits, unitMode(), toType, es);
    if (es.hadException())
        return create();

    float toValue = valueInSpecifiedUnits();
    length->newValueSpecifiedUnits(toType, WebCore::blend(fromValue, toValue, progress));
    return length;
}

void SVGLength::add(PassRefPtrWillBeRawPtr<SVGPropertyBase> other, SVGElement* contextElement)
{
    SVGLengthContext lengthContext(contextElement);

    setValue(value(lengthContext) + toSVGLength(other)->value(lengthContext), lengthContext, ASSERT_NO_EXCEPTION);
}

void SVGLength::calculateAnimatedValue(SVGAnimationElement* animationElement, float percentage, unsigned repeatCount, PassRefPtr<SVGPropertyBase> fromValue, PassRefPtr<SVGPropertyBase> toValue, PassRefPtr<SVGPropertyBase> toAtEndOfDurationValue, SVGElement* contextElement)
{
    RefPtr<SVGLength> fromLength = toSVGLength(fromValue);
    RefPtr<SVGLength> toLength = toSVGLength(toValue);
    RefPtr<SVGLength> toAtEndOfDurationLength = toSVGLength(toAtEndOfDurationValue);

    SVGLengthContext lengthContext(contextElement);
    float animatedNumber = value(lengthContext, IGNORE_EXCEPTION);
    animationElement->animateAdditiveNumber(percentage, repeatCount, fromLength->value(lengthContext, IGNORE_EXCEPTION), toLength->value(lengthContext, IGNORE_EXCEPTION), toAtEndOfDurationLength->value(lengthContext, IGNORE_EXCEPTION), animatedNumber);

    ASSERT(unitMode() == lengthModeForAnimatedLengthAttribute(animationElement->attributeName()));
    m_unitType = percentage < 0.5 ? fromLength->unitType() : toLength->unitType();
    setValue(animatedNumber, lengthContext, ASSERT_NO_EXCEPTION);
}

float SVGLength::calculateDistance(PassRefPtr<SVGPropertyBase> toValue, SVGElement* contextElement)
{
    SVGLengthContext lengthContext(contextElement);
    RefPtr<SVGLength> toLength = toSVGLength(toValue);

    return fabsf(toLength->value(lengthContext, IGNORE_EXCEPTION) - value(lengthContext, IGNORE_EXCEPTION));
}

}
