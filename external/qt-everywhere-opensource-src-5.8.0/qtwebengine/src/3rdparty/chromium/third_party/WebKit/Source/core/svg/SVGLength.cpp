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

#include "core/svg/SVGLength.h"

#include "core/SVGNames.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "core/css/parser/CSSParser.h"
#include "core/svg/SVGAnimationElement.h"
#include "wtf/MathExtras.h"
#include "wtf/text/WTFString.h"

namespace blink {

SVGLength::SVGLength(SVGLengthMode mode)
    : m_value(CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::UserUnits))
    , m_unitMode(static_cast<unsigned>(mode))
{
    ASSERT(unitMode() == mode);
}

SVGLength::SVGLength(const SVGLength& o)
    : m_value(o.m_value)
    , m_unitMode(o.m_unitMode)
{
}

DEFINE_TRACE(SVGLength)
{
    visitor->trace(m_value);
    SVGPropertyBase::trace(visitor);
}

SVGLength* SVGLength::clone() const
{
    return new SVGLength(*this);
}

SVGPropertyBase* SVGLength::cloneForAnimation(const String& value) const
{
    SVGLength* length = create();
    length->m_unitMode = m_unitMode;

    if (length->setValueAsString(value) != SVGParseStatus::NoError)
        length->m_value = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::UserUnits);

    return length;
}

bool SVGLength::operator==(const SVGLength& other) const
{
    return m_unitMode == other.m_unitMode && m_value == other.m_value;
}

float SVGLength::value(const SVGLengthContext& context) const
{
    return context.convertValueToUserUnits(
        m_value->getFloatValue(), unitMode(), m_value->typeWithCalcResolved());
}

void SVGLength::setValue(float value, const SVGLengthContext& context)
{
    m_value = CSSPrimitiveValue::create(
        context.convertValueFromUserUnits(value, unitMode(), m_value->typeWithCalcResolved()),
        m_value->typeWithCalcResolved());
}

bool isSupportedCSSUnitType(CSSPrimitiveValue::UnitType type)
{
    return (CSSPrimitiveValue::isLength(type) || type == CSSPrimitiveValue::UnitType::Number || type == CSSPrimitiveValue::UnitType::Percentage)
        && type != CSSPrimitiveValue::UnitType::QuirkyEms;
}

void SVGLength::setUnitType(CSSPrimitiveValue::UnitType type)
{
    ASSERT(isSupportedCSSUnitType(type));
    m_value = CSSPrimitiveValue::create(m_value->getFloatValue(), type);
}

float SVGLength::valueAsPercentage() const
{
    // LengthTypePercentage is represented with 100% = 100.0. Good for accuracy but could eventually be changed.
    if (m_value->isPercentage()) {
        // Note: This division is a source of floating point inaccuracy.
        return m_value->getFloatValue() / 100;
    }

    return m_value->getFloatValue();
}

float SVGLength::valueAsPercentage100() const
{
    // LengthTypePercentage is represented with 100% = 100.0. Good for accuracy but could eventually be changed.
    if (m_value->isPercentage())
        return m_value->getFloatValue();

    return m_value->getFloatValue() * 100;
}

float SVGLength::scaleByPercentage(float input) const
{
    float result = input * m_value->getFloatValue();
    if (m_value->isPercentage()) {
        // Delaying division by 100 as long as possible since it introduces floating point errors.
        result = result / 100;
    }
    return result;
}

SVGParsingError SVGLength::setValueAsString(const String& string)
{
    if (string.isEmpty()) {
        m_value = CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::UserUnits);
        return SVGParseStatus::NoError;
    }

    CSSParserContext svgParserContext(SVGAttributeMode, nullptr);
    CSSValue* parsed = CSSParser::parseSingleValue(CSSPropertyX, string, svgParserContext);
    if (!parsed || !parsed->isPrimitiveValue())
        return SVGParseStatus::ExpectedLength;

    CSSPrimitiveValue* newValue = toCSSPrimitiveValue(parsed);
    // TODO(fs): Enable calc for SVG lengths
    if (newValue->isCalculated() || !isSupportedCSSUnitType(newValue->typeWithCalcResolved()))
        return SVGParseStatus::ExpectedLength;

    m_value = newValue;
    return SVGParseStatus::NoError;
}

String SVGLength::valueAsString() const
{
    return m_value->customCSSText();
}

void SVGLength::newValueSpecifiedUnits(CSSPrimitiveValue::UnitType type, float value)
{
    m_value = CSSPrimitiveValue::create(value, type);
}

void SVGLength::convertToSpecifiedUnits(CSSPrimitiveValue::UnitType type, const SVGLengthContext& context)
{
    ASSERT(isSupportedCSSUnitType(type));

    float valueInUserUnits = value(context);
    m_value = CSSPrimitiveValue::create(
        context.convertValueFromUserUnits(valueInUserUnits, unitMode(), type),
        type);
}

SVGLengthMode SVGLength::lengthModeForAnimatedLengthAttribute(const QualifiedName& attrName)
{
    typedef HashMap<QualifiedName, SVGLengthMode> LengthModeForLengthAttributeMap;
    DEFINE_STATIC_LOCAL(LengthModeForLengthAttributeMap, s_lengthModeMap, ());

    if (s_lengthModeMap.isEmpty()) {
        s_lengthModeMap.set(SVGNames::xAttr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::yAttr, SVGLengthMode::Height);
        s_lengthModeMap.set(SVGNames::cxAttr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::cyAttr, SVGLengthMode::Height);
        s_lengthModeMap.set(SVGNames::dxAttr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::dyAttr, SVGLengthMode::Height);
        s_lengthModeMap.set(SVGNames::frAttr, SVGLengthMode::Other);
        s_lengthModeMap.set(SVGNames::fxAttr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::fyAttr, SVGLengthMode::Height);
        s_lengthModeMap.set(SVGNames::rAttr, SVGLengthMode::Other);
        s_lengthModeMap.set(SVGNames::rxAttr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::ryAttr, SVGLengthMode::Height);
        s_lengthModeMap.set(SVGNames::widthAttr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::heightAttr, SVGLengthMode::Height);
        s_lengthModeMap.set(SVGNames::x1Attr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::x2Attr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::y1Attr, SVGLengthMode::Height);
        s_lengthModeMap.set(SVGNames::y2Attr, SVGLengthMode::Height);
        s_lengthModeMap.set(SVGNames::refXAttr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::refYAttr, SVGLengthMode::Height);
        s_lengthModeMap.set(SVGNames::markerWidthAttr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::markerHeightAttr, SVGLengthMode::Height);
        s_lengthModeMap.set(SVGNames::textLengthAttr, SVGLengthMode::Width);
        s_lengthModeMap.set(SVGNames::startOffsetAttr, SVGLengthMode::Width);
    }

    if (s_lengthModeMap.contains(attrName))
        return s_lengthModeMap.get(attrName);

    return SVGLengthMode::Other;
}

bool SVGLength::negativeValuesForbiddenForAnimatedLengthAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, s_noNegativeValuesSet, ());

    if (s_noNegativeValuesSet.isEmpty()) {
        s_noNegativeValuesSet.add(SVGNames::frAttr);
        s_noNegativeValuesSet.add(SVGNames::rAttr);
        s_noNegativeValuesSet.add(SVGNames::rxAttr);
        s_noNegativeValuesSet.add(SVGNames::ryAttr);
        s_noNegativeValuesSet.add(SVGNames::widthAttr);
        s_noNegativeValuesSet.add(SVGNames::heightAttr);
        s_noNegativeValuesSet.add(SVGNames::markerWidthAttr);
        s_noNegativeValuesSet.add(SVGNames::markerHeightAttr);
        s_noNegativeValuesSet.add(SVGNames::textLengthAttr);
    }

    return s_noNegativeValuesSet.contains(attrName);
}

void SVGLength::add(SVGPropertyBase* other, SVGElement* contextElement)
{
    SVGLengthContext lengthContext(contextElement);
    setValue(value(lengthContext) + toSVGLength(other)->value(lengthContext), lengthContext);
}

void SVGLength::calculateAnimatedValue(SVGAnimationElement* animationElement,
    float percentage,
    unsigned repeatCount,
    SVGPropertyBase* fromValue,
    SVGPropertyBase* toValue,
    SVGPropertyBase* toAtEndOfDurationValue,
    SVGElement* contextElement)
{
    SVGLength* fromLength = toSVGLength(fromValue);
    SVGLength* toLength = toSVGLength(toValue);
    SVGLength* toAtEndOfDurationLength = toSVGLength(toAtEndOfDurationValue);

    SVGLengthContext lengthContext(contextElement);
    float animatedNumber = value(lengthContext);
    animationElement->animateAdditiveNumber(percentage, repeatCount, fromLength->value(lengthContext),
        toLength->value(lengthContext), toAtEndOfDurationLength->value(lengthContext), animatedNumber);

    ASSERT(unitMode() == lengthModeForAnimatedLengthAttribute(animationElement->attributeName()));

    CSSPrimitiveValue::UnitType newUnit = percentage < 0.5 ? fromLength->typeWithCalcResolved() : toLength->typeWithCalcResolved();
    animatedNumber = lengthContext.convertValueFromUserUnits(animatedNumber, unitMode(), newUnit);
    m_value = CSSPrimitiveValue::create(animatedNumber, newUnit);
}

float SVGLength::calculateDistance(SVGPropertyBase* toValue, SVGElement* contextElement)
{
    SVGLengthContext lengthContext(contextElement);
    SVGLength* toLength = toSVGLength(toValue);

    return fabsf(toLength->value(lengthContext) - value(lengthContext));
}

} // namespace blink
