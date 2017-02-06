// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSPropertyParserHelpers.h"

#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSValuePair.h"

namespace blink {

namespace CSSPropertyParserHelpers {

bool consumeCommaIncludingWhitespace(CSSParserTokenRange& range)
{
    CSSParserToken value = range.peek();
    if (value.type() != CommaToken)
        return false;
    range.consumeIncludingWhitespace();
    return true;
}

bool consumeSlashIncludingWhitespace(CSSParserTokenRange& range)
{
    CSSParserToken value = range.peek();
    if (value.type() != DelimiterToken || value.delimiter() != '/')
        return false;
    range.consumeIncludingWhitespace();
    return true;
}

CSSParserTokenRange consumeFunction(CSSParserTokenRange& range)
{
    ASSERT(range.peek().type() == FunctionToken);
    CSSParserTokenRange contents = range.consumeBlock();
    range.consumeWhitespace();
    contents.consumeWhitespace();
    return contents;
}

// TODO(rwlbuis): consider pulling in the parsing logic from CSSCalculationValue.cpp.
class CalcParser {
    STACK_ALLOCATED();

public:
    explicit CalcParser(CSSParserTokenRange& range, ValueRange valueRange = ValueRangeAll)
        : m_sourceRange(range)
        , m_range(range)
    {
        const CSSParserToken& token = range.peek();
        if (token.functionId() == CSSValueCalc || token.functionId() == CSSValueWebkitCalc)
            m_calcValue = CSSCalcValue::create(consumeFunction(m_range), valueRange);
    }

    const CSSCalcValue* value() const { return m_calcValue.get(); }
    CSSPrimitiveValue* consumeValue()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        return CSSPrimitiveValue::create(m_calcValue.release());
    }
    CSSPrimitiveValue* consumeNumber()
    {
        if (!m_calcValue)
            return nullptr;
        m_sourceRange = m_range;
        CSSPrimitiveValue::UnitType unitType = m_calcValue->isInt() ? CSSPrimitiveValue::UnitType::Integer : CSSPrimitiveValue::UnitType::Number;
        return CSSPrimitiveValue::create(m_calcValue->doubleValue(), unitType);
    }

    bool consumeNumberRaw(double& result)
    {
        if (!m_calcValue || m_calcValue->category() != CalcNumber)
            return false;
        m_sourceRange = m_range;
        result = m_calcValue->doubleValue();
        return true;
    }

private:
    CSSParserTokenRange& m_sourceRange;
    CSSParserTokenRange m_range;
    Member<CSSCalcValue> m_calcValue;
};

CSSPrimitiveValue* consumeInteger(CSSParserTokenRange& range, double minimumValue)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (token.numericValueType() == NumberValueType || token.numericValue() < minimumValue)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSPrimitiveValue::UnitType::Integer);
    }
    CalcParser calcParser(range);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() != CalcNumber || !calculation->isInt())
            return nullptr;
        double value = calculation->doubleValue();
        if (value < minimumValue)
            return nullptr;
        return calcParser.consumeNumber();
    }
    return nullptr;
}

CSSPrimitiveValue* consumePositiveInteger(CSSParserTokenRange& range)
{
    return consumeInteger(range, 1);
}

bool consumeNumberRaw(CSSParserTokenRange& range, double& result)
{
    if (range.peek().type() == NumberToken) {
        result = range.consumeIncludingWhitespace().numericValue();
        return true;
    }
    CalcParser calcParser(range, ValueRangeAll);
    return calcParser.consumeNumberRaw(result);
}

// TODO(timloh): Work out if this can just call consumeNumberRaw
CSSPrimitiveValue* consumeNumber(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == NumberToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), token.unitType());
    }
    CalcParser calcParser(range, ValueRangeAll);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        // TODO(rwlbuis) Calcs should not be subject to parse time range checks.
        // spec: https://drafts.csswg.org/css-values-3/#calc-range
        if (calculation->category() != CalcNumber || (valueRange == ValueRangeNonNegative && calculation->isNegative()))
            return nullptr;
        return calcParser.consumeNumber();
    }
    return nullptr;
}

inline bool shouldAcceptUnitlessLength(double value, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    // TODO(timloh): Presentational HTML attributes shouldn't use the CSS parser for lengths
    return value == 0
        || isUnitLessLengthParsingEnabledForMode(cssParserMode)
        || (cssParserMode == HTMLQuirksMode && unitless == UnitlessQuirk::Allow);
}

CSSPrimitiveValue* consumeLength(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        switch (token.unitType()) {
        case CSSPrimitiveValue::UnitType::QuirkyEms:
            if (cssParserMode != UASheetMode)
                return nullptr;
        /* fallthrough intentional */
        case CSSPrimitiveValue::UnitType::Ems:
        case CSSPrimitiveValue::UnitType::Rems:
        case CSSPrimitiveValue::UnitType::Chs:
        case CSSPrimitiveValue::UnitType::Exs:
        case CSSPrimitiveValue::UnitType::Pixels:
        case CSSPrimitiveValue::UnitType::Centimeters:
        case CSSPrimitiveValue::UnitType::Millimeters:
        case CSSPrimitiveValue::UnitType::Inches:
        case CSSPrimitiveValue::UnitType::Points:
        case CSSPrimitiveValue::UnitType::Picas:
        case CSSPrimitiveValue::UnitType::UserUnits:
        case CSSPrimitiveValue::UnitType::ViewportWidth:
        case CSSPrimitiveValue::UnitType::ViewportHeight:
        case CSSPrimitiveValue::UnitType::ViewportMin:
        case CSSPrimitiveValue::UnitType::ViewportMax:
            break;
        default:
            return nullptr;
        }
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), token.unitType());
    }
    if (token.type() == NumberToken) {
        if (!shouldAcceptUnitlessLength(token.numericValue(), cssParserMode, unitless)
            || (valueRange == ValueRangeNonNegative && token.numericValue() < 0))
            return nullptr;
        CSSPrimitiveValue::UnitType unitType = CSSPrimitiveValue::UnitType::Pixels;
        if (cssParserMode == SVGAttributeMode)
            unitType = CSSPrimitiveValue::UnitType::UserUnits;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), unitType);
    }
    if (cssParserMode == SVGAttributeMode)
        return nullptr;
    CalcParser calcParser(range, valueRange);
    if (calcParser.value() && calcParser.value()->category() == CalcLength)
        return calcParser.consumeValue();
    return nullptr;
}

CSSPrimitiveValue* consumePercent(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == PercentageToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSPrimitiveValue::UnitType::Percentage);
    }
    CalcParser calcParser(range, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcPercent)
            return calcParser.consumeValue();
    }
    return nullptr;
}

CSSPrimitiveValue* consumeLengthOrPercent(CSSParserTokenRange& range, CSSParserMode cssParserMode, ValueRange valueRange, UnitlessQuirk unitless)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken || token.type() == NumberToken)
        return consumeLength(range, cssParserMode, valueRange, unitless);
    if (token.type() == PercentageToken)
        return consumePercent(range, valueRange);
    CalcParser calcParser(range, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcLength || calculation->category() == CalcPercent || calculation->category() == CalcPercentLength)
            return calcParser.consumeValue();
    }
    return nullptr;
}

CSSPrimitiveValue* consumeAngle(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        switch (token.unitType()) {
        case CSSPrimitiveValue::UnitType::Degrees:
        case CSSPrimitiveValue::UnitType::Radians:
        case CSSPrimitiveValue::UnitType::Gradians:
        case CSSPrimitiveValue::UnitType::Turns:
            return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), token.unitType());
        default:
            return nullptr;
        }
    }
    if (token.type() == NumberToken && token.numericValue() == 0) {
        range.consumeIncludingWhitespace();
        return CSSPrimitiveValue::create(0, CSSPrimitiveValue::UnitType::Degrees);
    }
    CalcParser calcParser(range, ValueRangeAll);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcAngle)
            return calcParser.consumeValue();
    }
    return nullptr;
}

CSSPrimitiveValue* consumeTime(CSSParserTokenRange& range, ValueRange valueRange)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == DimensionToken) {
        if (valueRange == ValueRangeNonNegative && token.numericValue() < 0)
            return nullptr;
        CSSPrimitiveValue::UnitType unit = token.unitType();
        if (unit == CSSPrimitiveValue::UnitType::Milliseconds || unit == CSSPrimitiveValue::UnitType::Seconds)
            return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), token.unitType());
        return nullptr;
    }
    CalcParser calcParser(range, valueRange);
    if (const CSSCalcValue* calculation = calcParser.value()) {
        if (calculation->category() == CalcTime)
            return calcParser.consumeValue();
    }
    return nullptr;
}

CSSPrimitiveValue* consumeIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken)
        return nullptr;
    return CSSPrimitiveValue::createIdentifier(range.consumeIncludingWhitespace().id());
}

CSSPrimitiveValue* consumeIdentRange(CSSParserTokenRange& range, CSSValueID lower, CSSValueID upper)
{
    if (range.peek().id() < lower || range.peek().id() > upper)
        return nullptr;
    return consumeIdent(range);
}

CSSCustomIdentValue* consumeCustomIdent(CSSParserTokenRange& range)
{
    if (range.peek().type() != IdentToken || isCSSWideKeyword(range.peek().id()))
        return nullptr;
    return CSSCustomIdentValue::create(range.consumeIncludingWhitespace().value().toString());
}

CSSStringValue* consumeString(CSSParserTokenRange& range)
{
    if (range.peek().type() != StringToken)
        return nullptr;
    return CSSStringValue::create(range.consumeIncludingWhitespace().value().toString());
}

String consumeUrl(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == UrlToken) {
        range.consumeIncludingWhitespace();
        return token.value().toString();
    }
    if (token.functionId() == CSSValueUrl) {
        CSSParserTokenRange urlRange = range;
        CSSParserTokenRange urlArgs = urlRange.consumeBlock();
        const CSSParserToken& next = urlArgs.consumeIncludingWhitespace();
        if (next.type() == BadStringToken || !urlArgs.atEnd())
            return String();
        ASSERT(next.type() == StringToken);
        range = urlRange;
        range.consumeWhitespace();
        return next.value().toString();
    }

    return String();
}

static int clampRGBComponent(const CSSPrimitiveValue& value)
{
    double result = value.getDoubleValue();
    // TODO(timloh): Multiply by 2.55 and round instead of floor.
    if (value.isPercentage())
        result *= 2.56;
    return clampTo<int>(result, 0, 255);
}

static bool parseRGBParameters(CSSParserTokenRange& range, RGBA32& result, bool parseAlpha)
{
    ASSERT(range.peek().functionId() == CSSValueRgb || range.peek().functionId() == CSSValueRgba);
    CSSParserTokenRange args = consumeFunction(range);
    CSSPrimitiveValue* colorParameter = consumeInteger(args);
    if (!colorParameter)
        colorParameter = consumePercent(args, ValueRangeAll);
    if (!colorParameter)
        return false;
    const bool isPercent = colorParameter->isPercentage();
    int colorArray[3];
    colorArray[0] = clampRGBComponent(*colorParameter);
    for (int i = 1; i < 3; i++) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        colorParameter = isPercent ? consumePercent(args, ValueRangeAll) : consumeInteger(args);
        if (!colorParameter)
            return false;
        colorArray[i] = clampRGBComponent(*colorParameter);
    }
    if (parseAlpha) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        double alpha;
        if (!consumeNumberRaw(args, alpha))
            return false;
        // Convert the floating pointer number of alpha to an integer in the range [0, 256),
        // with an equal distribution across all 256 values.
        int alphaComponent = static_cast<int>(clampTo<double>(alpha, 0.0, 1.0) * nextafter(256.0, 0.0));
        result = makeRGBA(colorArray[0], colorArray[1], colorArray[2], alphaComponent);
    } else {
        result = makeRGB(colorArray[0], colorArray[1], colorArray[2]);
    }
    return args.atEnd();
}

static bool parseHSLParameters(CSSParserTokenRange& range, RGBA32& result, bool parseAlpha)
{
    ASSERT(range.peek().functionId() == CSSValueHsl || range.peek().functionId() == CSSValueHsla);
    CSSParserTokenRange args = consumeFunction(range);
    CSSPrimitiveValue* hslValue = consumeNumber(args, ValueRangeAll);
    if (!hslValue)
        return false;
    double colorArray[3];
    colorArray[0] = (((hslValue->getIntValue() % 360) + 360) % 360) / 360.0;
    for (int i = 1; i < 3; i++) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        hslValue = consumePercent(args, ValueRangeAll);
        if (!hslValue)
            return false;
        double doubleValue = hslValue->getDoubleValue();
        colorArray[i] = clampTo<double>(doubleValue, 0.0, 100.0) / 100.0; // Needs to be value between 0 and 1.0.
    }
    double alpha = 1.0;
    if (parseAlpha) {
        if (!consumeCommaIncludingWhitespace(args))
            return false;
        if (!consumeNumberRaw(args, alpha))
            return false;
        alpha = clampTo<double>(alpha, 0.0, 1.0);
    }
    result = makeRGBAFromHSLA(colorArray[0], colorArray[1], colorArray[2], alpha);
    return args.atEnd();
}

static bool parseHexColor(CSSParserTokenRange& range, RGBA32& result, bool acceptQuirkyColors)
{
    const CSSParserToken& token = range.peek();
    if (token.type() == HashToken) {
        if (!Color::parseHexColor(token.value(), result))
            return false;
    } else if (acceptQuirkyColors) {
        String color;
        if (token.type() == NumberToken || token.type() == DimensionToken) {
            if (token.numericValueType() != IntegerValueType
                || token.numericValue() < 0. || token.numericValue() >= 1000000.)
                return false;
            if (token.type() == NumberToken) // e.g. 112233
                color = String::format("%d", static_cast<int>(token.numericValue()));
            else // e.g. 0001FF
                color = String::number(static_cast<int>(token.numericValue())) + token.value().toString();
            while (color.length() < 6)
                color = "0" + color;
        } else if (token.type() == IdentToken) { // e.g. FF0000
            color = token.value().toString();
        }
        unsigned length = color.length();
        if (length != 3 && length != 6)
            return false;
        if (!Color::parseHexColor(color, result))
            return false;
    } else {
        return false;
    }
    range.consumeIncludingWhitespace();
    return true;
}

static bool parseColorFunction(CSSParserTokenRange& range, RGBA32& result)
{
    CSSValueID functionId = range.peek().functionId();
    if (functionId < CSSValueRgb || functionId > CSSValueHsla)
        return false;
    CSSParserTokenRange colorRange = range;
    if ((functionId <= CSSValueRgba && !parseRGBParameters(colorRange, result, functionId == CSSValueRgba))
        || (functionId >= CSSValueHsl && !parseHSLParameters(colorRange, result, functionId == CSSValueHsla)))
        return false;
    range = colorRange;
    return true;
}

CSSValue* consumeColor(CSSParserTokenRange& range, CSSParserMode cssParserMode, bool acceptQuirkyColors)
{
    CSSValueID id = range.peek().id();
    if (StyleColor::isColorKeyword(id)) {
        if (!isValueAllowedInMode(id, cssParserMode))
            return nullptr;
        return consumeIdent(range);
    }
    RGBA32 color = Color::transparent;
    if (!parseHexColor(range, color, acceptQuirkyColors) && !parseColorFunction(range, color))
        return nullptr;
    return CSSColorValue::create(color);
}

static CSSPrimitiveValue* consumePositionComponent(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    if (range.peek().type() == IdentToken)
        return consumeIdent<CSSValueLeft, CSSValueTop, CSSValueBottom, CSSValueRight, CSSValueCenter>(range);
    return consumeLengthOrPercent(range, cssParserMode, ValueRangeAll, unitless);
}

static bool isHorizontalPositionKeywordOnly(const CSSPrimitiveValue& value)
{
    return value.isValueID() && (value.getValueID() == CSSValueLeft || value.getValueID() == CSSValueRight);
}

static bool isVerticalPositionKeywordOnly(const CSSPrimitiveValue& value)
{
    return value.isValueID() && (value.getValueID() == CSSValueTop || value.getValueID() == CSSValueBottom);
}

static void positionFromOneValue(CSSPrimitiveValue* value, CSSValue*& resultX, CSSValue*& resultY)
{
    bool valueAppliesToYAxisOnly = isVerticalPositionKeywordOnly(*value);
    resultX = value;
    resultY = CSSPrimitiveValue::createIdentifier(CSSValueCenter);
    if (valueAppliesToYAxisOnly)
        std::swap(resultX, resultY);
}

static bool positionFromTwoValues(CSSPrimitiveValue* value1, CSSPrimitiveValue* value2,
    CSSValue*& resultX, CSSValue*& resultY)
{
    bool mustOrderAsXY = isHorizontalPositionKeywordOnly(*value1) || isVerticalPositionKeywordOnly(*value2)
        || !value1->isValueID() || !value2->isValueID();
    bool mustOrderAsYX = isVerticalPositionKeywordOnly(*value1) || isHorizontalPositionKeywordOnly(*value2);
    if (mustOrderAsXY && mustOrderAsYX)
        return false;
    resultX = value1;
    resultY = value2;
    if (mustOrderAsYX)
        std::swap(resultX, resultY);
    return true;
}

static bool positionFromThreeOrFourValues(CSSPrimitiveValue** values, CSSValue*& resultX, CSSValue*& resultY)
{
    CSSPrimitiveValue* center = nullptr;
    for (int i = 0; values[i]; i++) {
        CSSPrimitiveValue* currentValue = values[i];
        if (!currentValue->isValueID())
            return false;
        CSSValueID id = currentValue->getValueID();

        if (id == CSSValueCenter) {
            if (center)
                return false;
            center = currentValue;
            continue;
        }

        CSSValue* result = nullptr;
        if (values[i + 1] && !values[i + 1]->isValueID()) {
            result = CSSValuePair::create(currentValue, values[++i], CSSValuePair::KeepIdenticalValues);
        } else {
            result = currentValue;
        }

        if (id == CSSValueLeft || id == CSSValueRight) {
            if (resultX)
                return false;
            resultX = result;
        } else {
            ASSERT(id == CSSValueTop || id == CSSValueBottom);
            if (resultY)
                return false;
            resultY = result;
        }
    }

    if (center) {
        ASSERT(resultX || resultY);
        if (resultX && resultY)
            return false;
        if (!resultX)
            resultX = center;
        else
            resultY = center;
    }

    ASSERT(resultX && resultY);
    return true;
}

// TODO(timloh): This may consume from the range upon failure. The background
// shorthand works around it, but we should just fix it here.
bool consumePosition(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, CSSValue*& resultX, CSSValue*& resultY)
{
    CSSPrimitiveValue* value1 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value1)
        return false;

    CSSPrimitiveValue* value2 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value2) {
        positionFromOneValue(value1, resultX, resultY);
        return true;
    }

    CSSPrimitiveValue* value3 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value3)
        return positionFromTwoValues(value1, value2, resultX, resultY);

    CSSPrimitiveValue* value4 = consumePositionComponent(range, cssParserMode, unitless);
    CSSPrimitiveValue* values[5];
    values[0] = value1;
    values[1] = value2;
    values[2] = value3;
    values[3] = value4;
    values[4] = nullptr;
    return positionFromThreeOrFourValues(values, resultX, resultY);
}

CSSValuePair* consumePosition(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless)
{
    CSSValue* resultX = nullptr;
    CSSValue* resultY = nullptr;
    if (consumePosition(range, cssParserMode, unitless, resultX, resultY))
        return CSSValuePair::create(resultX, resultY, CSSValuePair::KeepIdenticalValues);
    return nullptr;
}

bool consumeOneOrTwoValuedPosition(CSSParserTokenRange& range, CSSParserMode cssParserMode, UnitlessQuirk unitless, CSSValue*& resultX, CSSValue*& resultY)
{
    CSSPrimitiveValue* value1 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value1)
        return false;
    CSSPrimitiveValue* value2 = consumePositionComponent(range, cssParserMode, unitless);
    if (!value2) {
        positionFromOneValue(value1, resultX, resultY);
        return true;
    }
    return positionFromTwoValues(value1, value2, resultX, resultY);
}

} // namespace CSSPropertyParserHelpers

} // namespace blink
