/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSCalculationValue.h"

#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/resolver/StyleResolver.h"
#include "wtf/MathExtras.h"
#include "wtf/text/StringBuilder.h"

static const int maxExpressionDepth = 100;

enum ParseState {
    OK,
    TooDeep,
    NoMoreTokens
};

namespace blink {

static CalculationCategory unitCategory(CSSPrimitiveValue::UnitType type)
{
    switch (type) {
    case CSSPrimitiveValue::UnitType::Number:
    case CSSPrimitiveValue::UnitType::Integer:
        return CalcNumber;
    case CSSPrimitiveValue::UnitType::Percentage:
        return CalcPercent;
    case CSSPrimitiveValue::UnitType::Ems:
    case CSSPrimitiveValue::UnitType::Exs:
    case CSSPrimitiveValue::UnitType::Pixels:
    case CSSPrimitiveValue::UnitType::Centimeters:
    case CSSPrimitiveValue::UnitType::Millimeters:
    case CSSPrimitiveValue::UnitType::Inches:
    case CSSPrimitiveValue::UnitType::Points:
    case CSSPrimitiveValue::UnitType::Picas:
    case CSSPrimitiveValue::UnitType::UserUnits:
    case CSSPrimitiveValue::UnitType::Rems:
    case CSSPrimitiveValue::UnitType::Chs:
    case CSSPrimitiveValue::UnitType::ViewportWidth:
    case CSSPrimitiveValue::UnitType::ViewportHeight:
    case CSSPrimitiveValue::UnitType::ViewportMin:
    case CSSPrimitiveValue::UnitType::ViewportMax:
        return CalcLength;
    case CSSPrimitiveValue::UnitType::Degrees:
    case CSSPrimitiveValue::UnitType::Gradians:
    case CSSPrimitiveValue::UnitType::Radians:
    case CSSPrimitiveValue::UnitType::Turns:
        return CalcAngle;
    case CSSPrimitiveValue::UnitType::Milliseconds:
    case CSSPrimitiveValue::UnitType::Seconds:
        return CalcTime;
    case CSSPrimitiveValue::UnitType::Hertz:
    case CSSPrimitiveValue::UnitType::Kilohertz:
        return CalcFrequency;
    default:
        return CalcOther;
    }
}

static bool hasDoubleValue(CSSPrimitiveValue::UnitType type)
{
    switch (type) {
    case CSSPrimitiveValue::UnitType::Number:
    case CSSPrimitiveValue::UnitType::Percentage:
    case CSSPrimitiveValue::UnitType::Ems:
    case CSSPrimitiveValue::UnitType::Exs:
    case CSSPrimitiveValue::UnitType::Chs:
    case CSSPrimitiveValue::UnitType::Rems:
    case CSSPrimitiveValue::UnitType::Pixels:
    case CSSPrimitiveValue::UnitType::Centimeters:
    case CSSPrimitiveValue::UnitType::Millimeters:
    case CSSPrimitiveValue::UnitType::Inches:
    case CSSPrimitiveValue::UnitType::Points:
    case CSSPrimitiveValue::UnitType::Picas:
    case CSSPrimitiveValue::UnitType::UserUnits:
    case CSSPrimitiveValue::UnitType::Degrees:
    case CSSPrimitiveValue::UnitType::Radians:
    case CSSPrimitiveValue::UnitType::Gradians:
    case CSSPrimitiveValue::UnitType::Turns:
    case CSSPrimitiveValue::UnitType::Milliseconds:
    case CSSPrimitiveValue::UnitType::Seconds:
    case CSSPrimitiveValue::UnitType::Hertz:
    case CSSPrimitiveValue::UnitType::Kilohertz:
    case CSSPrimitiveValue::UnitType::ViewportWidth:
    case CSSPrimitiveValue::UnitType::ViewportHeight:
    case CSSPrimitiveValue::UnitType::ViewportMin:
    case CSSPrimitiveValue::UnitType::ViewportMax:
    case CSSPrimitiveValue::UnitType::DotsPerPixel:
    case CSSPrimitiveValue::UnitType::DotsPerInch:
    case CSSPrimitiveValue::UnitType::DotsPerCentimeter:
    case CSSPrimitiveValue::UnitType::Fraction:
    case CSSPrimitiveValue::UnitType::Integer:
        return true;
    case CSSPrimitiveValue::UnitType::Unknown:
    case CSSPrimitiveValue::UnitType::Calc:
    case CSSPrimitiveValue::UnitType::CalcPercentageWithNumber:
    case CSSPrimitiveValue::UnitType::CalcPercentageWithLength:
    case CSSPrimitiveValue::UnitType::ValueID:
    case CSSPrimitiveValue::UnitType::QuirkyEms:
        return false;
    };
    ASSERT_NOT_REACHED();
    return false;
}

static String buildCSSText(const String& expression)
{
    StringBuilder result;
    result.append("calc");
    bool expressionHasSingleTerm = expression[0] != '(';
    if (expressionHasSingleTerm)
        result.append('(');
    result.append(expression);
    if (expressionHasSingleTerm)
        result.append(')');
    return result.toString();
}

String CSSCalcValue::customCSSText() const
{
    return buildCSSText(m_expression->customCSSText());
}

bool CSSCalcValue::equals(const CSSCalcValue& other) const
{
    return compareCSSValuePtr(m_expression, other.m_expression);
}

double CSSCalcValue::clampToPermittedRange(double value) const
{
    return m_nonNegative && value < 0 ? 0 : value;
}

double CSSCalcValue::doubleValue() const
{
    return clampToPermittedRange(m_expression->doubleValue());
}

double CSSCalcValue::computeLengthPx(const CSSToLengthConversionData& conversionData) const
{
    return clampToPermittedRange(m_expression->computeLengthPx(conversionData));
}

class CSSCalcPrimitiveValue final : public CSSCalcExpressionNode {
public:

    static CSSCalcPrimitiveValue* create(CSSPrimitiveValue* value, bool isInteger)
    {
        return new CSSCalcPrimitiveValue(value, isInteger);
    }

    static CSSCalcPrimitiveValue* create(double value, CSSPrimitiveValue::UnitType type, bool isInteger)
    {
        if (std::isnan(value) || std::isinf(value))
            return nullptr;
        return new CSSCalcPrimitiveValue(CSSPrimitiveValue::create(value, type), isInteger);
    }

    bool isZero() const override
    {
        return !m_value->getDoubleValue();
    }

    String customCSSText() const override
    {
        return m_value->cssText();
    }

    void accumulatePixelsAndPercent(const CSSToLengthConversionData& conversionData, PixelsAndPercent& value, float multiplier) const override
    {
        switch (m_category) {
        case CalcLength:
            value.pixels += m_value->computeLength<float>(conversionData) * multiplier;
            break;
        case CalcPercent:
            ASSERT(m_value->isPercentage());
            value.percent += m_value->getDoubleValue() * multiplier;
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    double doubleValue() const override
    {
        if (hasDoubleValue(typeWithCalcResolved()))
            return m_value->getDoubleValue();
        ASSERT_NOT_REACHED();
        return 0;
    }

    double computeLengthPx(const CSSToLengthConversionData& conversionData) const override
    {
        switch (m_category) {
        case CalcLength:
            return m_value->computeLength<double>(conversionData);
        case CalcNumber:
        case CalcPercent:
            return m_value->getDoubleValue();
        case CalcAngle:
        case CalcFrequency:
        case CalcPercentLength:
        case CalcPercentNumber:
        case CalcTime:
        case CalcOther:
            ASSERT_NOT_REACHED();
            break;
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    void accumulateLengthArray(CSSLengthArray& lengthArray, double multiplier) const override
    {
        ASSERT(category() != CalcNumber);
        m_value->accumulateLengthArray(lengthArray, multiplier);
    }

    bool equals(const CSSCalcExpressionNode& other) const override
    {
        if (getType() != other.getType())
            return false;

        return compareCSSValuePtr(m_value, static_cast<const CSSCalcPrimitiveValue&>(other).m_value);
    }

    Type getType() const override { return CssCalcPrimitiveValue; }
    CSSPrimitiveValue::UnitType typeWithCalcResolved() const override
    {
        return m_value->typeWithCalcResolved();
    }


    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_value);
        CSSCalcExpressionNode::trace(visitor);
    }

private:
    CSSCalcPrimitiveValue(CSSPrimitiveValue* value, bool isInteger)
        : CSSCalcExpressionNode(unitCategory(value->typeWithCalcResolved()), isInteger)
        , m_value(value)
    {
    }

    Member<CSSPrimitiveValue> m_value;
};

static const CalculationCategory addSubtractResult[CalcOther][CalcOther] = {
//                        CalcNumber         CalcLength         CalcPercent        CalcPercentNumber  CalcPercentLength  CalcAngle  CalcTime   CalcFrequency
/* CalcNumber */        { CalcNumber,        CalcOther,         CalcPercentNumber, CalcPercentNumber, CalcOther,         CalcOther, CalcOther, CalcOther     },
/* CalcLength */        { CalcOther,         CalcLength,        CalcPercentLength, CalcOther,         CalcPercentLength, CalcOther, CalcOther, CalcOther     },
/* CalcPercent */       { CalcPercentNumber, CalcPercentLength, CalcPercent,       CalcPercentNumber, CalcPercentLength, CalcOther, CalcOther, CalcOther     },
/* CalcPercentNumber */ { CalcPercentNumber, CalcOther,         CalcPercentNumber, CalcPercentNumber, CalcOther,         CalcOther, CalcOther, CalcOther     },
/* CalcPercentLength */ { CalcOther,         CalcPercentLength, CalcPercentLength, CalcOther,         CalcPercentLength, CalcOther, CalcOther, CalcOther     },
/* CalcAngle  */        { CalcOther,         CalcOther,         CalcOther,         CalcOther,         CalcOther,         CalcAngle, CalcOther, CalcOther     },
/* CalcTime */          { CalcOther,         CalcOther,         CalcOther,         CalcOther,         CalcOther,         CalcOther, CalcTime,  CalcOther     },
/* CalcFrequency */     { CalcOther,         CalcOther,         CalcOther,         CalcOther,         CalcOther,         CalcOther, CalcOther, CalcFrequency }
};

static CalculationCategory determineCategory(const CSSCalcExpressionNode& leftSide, const CSSCalcExpressionNode& rightSide, CalcOperator op)
{
    CalculationCategory leftCategory = leftSide.category();
    CalculationCategory rightCategory = rightSide.category();

    if (leftCategory == CalcOther || rightCategory == CalcOther)
        return CalcOther;

    switch (op) {
    case CalcAdd:
    case CalcSubtract:
        return addSubtractResult[leftCategory][rightCategory];
    case CalcMultiply:
        if (leftCategory != CalcNumber && rightCategory != CalcNumber)
            return CalcOther;
        return leftCategory == CalcNumber ? rightCategory : leftCategory;
    case CalcDivide:
        if (rightCategory != CalcNumber || rightSide.isZero())
            return CalcOther;
        return leftCategory;
    }

    ASSERT_NOT_REACHED();
    return CalcOther;
}

static bool isIntegerResult(const CSSCalcExpressionNode* leftSide, const CSSCalcExpressionNode* rightSide, CalcOperator op)
{
    // Not testing for actual integer values.
    // Performs W3C spec's type checking for calc integers.
    // http://www.w3.org/TR/css3-values/#calc-type-checking
    return op != CalcDivide && leftSide->isInteger() && rightSide->isInteger();
}

class CSSCalcBinaryOperation final : public CSSCalcExpressionNode {
public:
    static CSSCalcExpressionNode* create(CSSCalcExpressionNode* leftSide, CSSCalcExpressionNode* rightSide, CalcOperator op)
    {
        ASSERT(leftSide->category() != CalcOther && rightSide->category() != CalcOther);

        CalculationCategory newCategory = determineCategory(*leftSide, *rightSide, op);
        if (newCategory == CalcOther)
            return nullptr;

        return new CSSCalcBinaryOperation(leftSide, rightSide, op, newCategory);
    }

    static CSSCalcExpressionNode* createSimplified(CSSCalcExpressionNode* leftSide, CSSCalcExpressionNode* rightSide, CalcOperator op)
    {
        CalculationCategory leftCategory = leftSide->category();
        CalculationCategory rightCategory = rightSide->category();
        ASSERT(leftCategory != CalcOther && rightCategory != CalcOther);

        bool isInteger = isIntegerResult(leftSide, rightSide, op);

        // Simplify numbers.
        if (leftCategory == CalcNumber && rightCategory == CalcNumber) {
            return CSSCalcPrimitiveValue::create(evaluateOperator(leftSide->doubleValue(), rightSide->doubleValue(), op), CSSPrimitiveValue::UnitType::Number, isInteger);
        }

        // Simplify addition and subtraction between same types.
        if (op == CalcAdd || op == CalcSubtract) {
            if (leftCategory == rightSide->category()) {
                CSSPrimitiveValue::UnitType leftType = leftSide->typeWithCalcResolved();
                if (hasDoubleValue(leftType)) {
                    CSSPrimitiveValue::UnitType rightType = rightSide->typeWithCalcResolved();
                    if (leftType == rightType)
                        return CSSCalcPrimitiveValue::create(evaluateOperator(leftSide->doubleValue(), rightSide->doubleValue(), op), leftType, isInteger);
                    CSSPrimitiveValue::UnitCategory leftUnitCategory = CSSPrimitiveValue::unitTypeToUnitCategory(leftType);
                    if (leftUnitCategory != CSSPrimitiveValue::UOther && leftUnitCategory == CSSPrimitiveValue::unitTypeToUnitCategory(rightType)) {
                        CSSPrimitiveValue::UnitType canonicalType = CSSPrimitiveValue::canonicalUnitTypeForCategory(leftUnitCategory);
                        if (canonicalType != CSSPrimitiveValue::UnitType::Unknown) {
                            double leftValue = leftSide->doubleValue() * CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(leftType);
                            double rightValue = rightSide->doubleValue() * CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(rightType);
                            return CSSCalcPrimitiveValue::create(evaluateOperator(leftValue, rightValue, op), canonicalType, isInteger);
                        }
                    }
                }
            }
        } else {
            // Simplify multiplying or dividing by a number for simplifiable types.
            ASSERT(op == CalcMultiply || op == CalcDivide);
            CSSCalcExpressionNode* numberSide = getNumberSide(leftSide, rightSide);
            if (!numberSide)
                return create(leftSide, rightSide, op);
            if (numberSide == leftSide && op == CalcDivide)
                return nullptr;
            CSSCalcExpressionNode* otherSide = leftSide == numberSide ? rightSide : leftSide;

            double number = numberSide->doubleValue();
            if (std::isnan(number) || std::isinf(number))
                return nullptr;
            if (op == CalcDivide && !number)
                return nullptr;

            CSSPrimitiveValue::UnitType otherType = otherSide->typeWithCalcResolved();
            if (hasDoubleValue(otherType))
                return CSSCalcPrimitiveValue::create(evaluateOperator(otherSide->doubleValue(), number, op), otherType, isInteger);
        }

        return create(leftSide, rightSide, op);
    }

    bool isZero() const override
    {
        return !doubleValue();
    }

    void accumulatePixelsAndPercent(const CSSToLengthConversionData& conversionData, PixelsAndPercent& value, float multiplier) const override
    {
        switch (m_operator) {
        case CalcAdd:
            m_leftSide->accumulatePixelsAndPercent(conversionData, value, multiplier);
            m_rightSide->accumulatePixelsAndPercent(conversionData, value, multiplier);
            break;
        case CalcSubtract:
            m_leftSide->accumulatePixelsAndPercent(conversionData, value, multiplier);
            m_rightSide->accumulatePixelsAndPercent(conversionData, value, -multiplier);
            break;
        case CalcMultiply:
            ASSERT((m_leftSide->category() == CalcNumber) != (m_rightSide->category() == CalcNumber));
            if (m_leftSide->category() == CalcNumber)
                m_rightSide->accumulatePixelsAndPercent(conversionData, value, multiplier * m_leftSide->doubleValue());
            else
                m_leftSide->accumulatePixelsAndPercent(conversionData, value, multiplier * m_rightSide->doubleValue());
            break;
        case CalcDivide:
            ASSERT(m_rightSide->category() == CalcNumber);
            m_leftSide->accumulatePixelsAndPercent(conversionData, value, multiplier / m_rightSide->doubleValue());
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    double doubleValue() const override
    {
        return evaluate(m_leftSide->doubleValue(), m_rightSide->doubleValue());
    }

    double computeLengthPx(const CSSToLengthConversionData& conversionData) const override
    {
        const double leftValue = m_leftSide->computeLengthPx(conversionData);
        const double rightValue = m_rightSide->computeLengthPx(conversionData);
        return evaluate(leftValue, rightValue);
    }

    void accumulateLengthArray(CSSLengthArray& lengthArray, double multiplier) const override
    {
        switch (m_operator) {
        case CalcAdd:
            m_leftSide->accumulateLengthArray(lengthArray, multiplier);
            m_rightSide->accumulateLengthArray(lengthArray, multiplier);
            break;
        case CalcSubtract:
            m_leftSide->accumulateLengthArray(lengthArray, multiplier);
            m_rightSide->accumulateLengthArray(lengthArray, -multiplier);
            break;
        case CalcMultiply:
            ASSERT((m_leftSide->category() == CalcNumber) != (m_rightSide->category() == CalcNumber));
            if (m_leftSide->category() == CalcNumber)
                m_rightSide->accumulateLengthArray(lengthArray, multiplier * m_leftSide->doubleValue());
            else
                m_leftSide->accumulateLengthArray(lengthArray, multiplier * m_rightSide->doubleValue());
            break;
        case CalcDivide:
            ASSERT(m_rightSide->category() == CalcNumber);
            m_leftSide->accumulateLengthArray(lengthArray, multiplier / m_rightSide->doubleValue());
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    static String buildCSSText(const String& leftExpression, const String& rightExpression, CalcOperator op)
    {
        StringBuilder result;
        result.append('(');
        result.append(leftExpression);
        result.append(' ');
        result.append(static_cast<char>(op));
        result.append(' ');
        result.append(rightExpression);
        result.append(')');

        return result.toString();
    }

    String customCSSText() const override
    {
        return buildCSSText(m_leftSide->customCSSText(), m_rightSide->customCSSText(), m_operator);
    }

    bool equals(const CSSCalcExpressionNode& exp) const override
    {
        if (getType() != exp.getType())
            return false;

        const CSSCalcBinaryOperation& other = static_cast<const CSSCalcBinaryOperation&>(exp);
        return compareCSSValuePtr(m_leftSide, other.m_leftSide)
            && compareCSSValuePtr(m_rightSide, other.m_rightSide)
            && m_operator == other.m_operator;
    }

    Type getType() const override { return CssCalcBinaryOperation; }

    CSSPrimitiveValue::UnitType typeWithCalcResolved() const override
    {
        switch (m_category) {
        case CalcNumber:
            ASSERT(m_leftSide->category() == CalcNumber && m_rightSide->category() == CalcNumber);
            return CSSPrimitiveValue::UnitType::Number;
        case CalcLength:
        case CalcPercent: {
            if (m_leftSide->category() == CalcNumber)
                return m_rightSide->typeWithCalcResolved();
            if (m_rightSide->category() == CalcNumber)
                return m_leftSide->typeWithCalcResolved();
            CSSPrimitiveValue::UnitType leftType = m_leftSide->typeWithCalcResolved();
            if (leftType == m_rightSide->typeWithCalcResolved())
                return leftType;
            return CSSPrimitiveValue::UnitType::Unknown;
        }
        case CalcAngle:
            return CSSPrimitiveValue::UnitType::Degrees;
        case CalcTime:
            return CSSPrimitiveValue::UnitType::Milliseconds;
        case CalcFrequency:
            return CSSPrimitiveValue::UnitType::Hertz;
        case CalcPercentLength:
        case CalcPercentNumber:
        case CalcOther:
            return CSSPrimitiveValue::UnitType::Unknown;
        }
        ASSERT_NOT_REACHED();
        return CSSPrimitiveValue::UnitType::Unknown;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_leftSide);
        visitor->trace(m_rightSide);
        CSSCalcExpressionNode::trace(visitor);
    }

private:
    CSSCalcBinaryOperation(CSSCalcExpressionNode* leftSide, CSSCalcExpressionNode* rightSide, CalcOperator op, CalculationCategory category)
        : CSSCalcExpressionNode(category, isIntegerResult(leftSide, rightSide, op))
        , m_leftSide(leftSide)
        , m_rightSide(rightSide)
        , m_operator(op)
    {
    }

    static CSSCalcExpressionNode* getNumberSide(CSSCalcExpressionNode* leftSide, CSSCalcExpressionNode* rightSide)
    {
        if (leftSide->category() == CalcNumber)
            return leftSide;
        if (rightSide->category() == CalcNumber)
            return rightSide;
        return nullptr;
    }

    double evaluate(double leftSide, double rightSide) const
    {
        return evaluateOperator(leftSide, rightSide, m_operator);
    }

    static double evaluateOperator(double leftValue, double rightValue, CalcOperator op)
    {
        switch (op) {
        case CalcAdd:
            return leftValue + rightValue;
        case CalcSubtract:
            return leftValue - rightValue;
        case CalcMultiply:
            return leftValue * rightValue;
        case CalcDivide:
            if (rightValue)
                return leftValue / rightValue;
            return std::numeric_limits<double>::quiet_NaN();
        }
        return 0;
    }

    const Member<CSSCalcExpressionNode> m_leftSide;
    const Member<CSSCalcExpressionNode> m_rightSide;
    const CalcOperator m_operator;
};

static ParseState checkDepthAndIndex(int* depth, CSSParserTokenRange tokens)
{
    (*depth)++;
    if (tokens.atEnd())
        return NoMoreTokens;
    if (*depth > maxExpressionDepth)
        return TooDeep;
    return OK;
}

class CSSCalcExpressionNodeParser {
    STACK_ALLOCATED();
public:
    CSSCalcExpressionNode* parseCalc(CSSParserTokenRange tokens)
    {
        Value result;
        tokens.consumeWhitespace();
        bool ok = parseValueExpression(tokens, 0, &result);
        if (!ok || !tokens.atEnd())
            return nullptr;
        return result.value;
    }

private:
    struct Value {
        STACK_ALLOCATED();
    public:
        Member<CSSCalcExpressionNode> value;
    };

    char operatorValue(const CSSParserToken& token)
    {
        if (token.type() == DelimiterToken)
            return token.delimiter();
        return 0;
    }

    bool parseValue(CSSParserTokenRange& tokens, Value* result)
    {
        CSSParserToken token = tokens.consumeIncludingWhitespace();
        if (!(token.type() == NumberToken || token.type() == PercentageToken || token.type() == DimensionToken))
            return false;

        CSSPrimitiveValue::UnitType type = token.unitType();
        if (unitCategory(type) == CalcOther)
            return false;

        result->value = CSSCalcPrimitiveValue::create(
            CSSPrimitiveValue::create(token.numericValue(), type), token.numericValueType() == IntegerValueType);

        return true;
    }

    bool parseValueTerm(CSSParserTokenRange& tokens, int depth, Value* result)
    {
        if (checkDepthAndIndex(&depth, tokens) != OK)
            return false;

        if (tokens.peek().type() == LeftParenthesisToken || tokens.peek().functionId() == CSSValueCalc) {
            CSSParserTokenRange innerRange = tokens.consumeBlock();
            tokens.consumeWhitespace();
            innerRange.consumeWhitespace();
            return parseValueExpression(innerRange, depth, result);
        }

        return parseValue(tokens, result);
    }

    bool parseValueMultiplicativeExpression(CSSParserTokenRange& tokens, int depth, Value* result)
    {
        if (checkDepthAndIndex(&depth, tokens) != OK)
            return false;

        if (!parseValueTerm(tokens, depth, result))
            return false;

        while (!tokens.atEnd()) {
            char operatorCharacter = operatorValue(tokens.peek());
            if (operatorCharacter != CalcMultiply && operatorCharacter != CalcDivide)
                break;
            tokens.consumeIncludingWhitespace();

            Value rhs;
            if (!parseValueTerm(tokens, depth, &rhs))
                return false;

            result->value = CSSCalcBinaryOperation::createSimplified(result->value, rhs.value, static_cast<CalcOperator>(operatorCharacter));
            if (!result->value)
                return false;
        }

        return true;
    }

    bool parseAdditiveValueExpression(CSSParserTokenRange& tokens, int depth, Value* result)
    {
        if (checkDepthAndIndex(&depth, tokens) != OK)
            return false;

        if (!parseValueMultiplicativeExpression(tokens, depth, result))
            return false;

        while (!tokens.atEnd()) {
            char operatorCharacter = operatorValue(tokens.peek());
            if (operatorCharacter != CalcAdd && operatorCharacter != CalcSubtract)
                break;
            if ((&tokens.peek() - 1)->type() != WhitespaceToken)
                return false; // calc(1px+ 2px) is invalid
            tokens.consume();
            if (tokens.peek().type() != WhitespaceToken)
                return false; // calc(1px +2px) is invalid
            tokens.consumeIncludingWhitespace();

            Value rhs;
            if (!parseValueMultiplicativeExpression(tokens, depth, &rhs))
                return false;

            result->value = CSSCalcBinaryOperation::createSimplified(result->value, rhs.value, static_cast<CalcOperator>(operatorCharacter));
            if (!result->value)
                return false;
        }

        return true;
    }

    bool parseValueExpression(CSSParserTokenRange& tokens, int depth, Value* result)
    {
        return parseAdditiveValueExpression(tokens, depth, result);
    }
};

CSSCalcExpressionNode* CSSCalcValue::createExpressionNode(CSSPrimitiveValue* value, bool isInteger)
{
    return CSSCalcPrimitiveValue::create(value, isInteger);
}

CSSCalcExpressionNode* CSSCalcValue::createExpressionNode(CSSCalcExpressionNode* leftSide, CSSCalcExpressionNode* rightSide, CalcOperator op)
{
    return CSSCalcBinaryOperation::create(leftSide, rightSide, op);
}

CSSCalcExpressionNode* CSSCalcValue::createExpressionNode(double pixels, double percent)
{
    return createExpressionNode(
        createExpressionNode(CSSPrimitiveValue::create(pixels, CSSPrimitiveValue::UnitType::Pixels), pixels == trunc(pixels)),
        createExpressionNode(CSSPrimitiveValue::create(percent, CSSPrimitiveValue::UnitType::Percentage), percent == trunc(percent)),
        CalcAdd);
}

CSSCalcValue* CSSCalcValue::create(const CSSParserTokenRange& tokens, ValueRange range)
{
    CSSCalcExpressionNodeParser parser;
    CSSCalcExpressionNode* expression = parser.parseCalc(tokens);

    return expression ? new CSSCalcValue(expression, range) : nullptr;
}

CSSCalcValue* CSSCalcValue::create(CSSCalcExpressionNode* expression, ValueRange range)
{
    return new CSSCalcValue(expression, range);
}

} // namespace blink
