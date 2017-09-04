// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSVariableParser.h"

#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

bool CSSVariableParser::isValidVariableName(const CSSParserToken& token) {
  if (token.type() != IdentToken)
    return false;

  StringView value = token.value();
  return value.length() >= 2 && value[0] == '-' && value[1] == '-';
}

bool CSSVariableParser::isValidVariableName(const String& string) {
  return string.length() >= 2 && string[0] == '-' && string[1] == '-';
}

bool isValidVariableReference(CSSParserTokenRange, bool& hasAtApplyRule);

bool classifyBlock(CSSParserTokenRange range,
                   bool& hasReferences,
                   bool& hasAtApplyRule,
                   bool isTopLevelBlock = true) {
  while (!range.atEnd()) {
    if (range.peek().getBlockType() == CSSParserToken::BlockStart) {
      const CSSParserToken& token = range.peek();
      CSSParserTokenRange block = range.consumeBlock();
      if (token.functionId() == CSSValueVar) {
        if (!isValidVariableReference(block, hasAtApplyRule))
          return false;  // Bail if any references are invalid
        hasReferences = true;
        continue;
      }
      if (!classifyBlock(block, hasReferences, hasAtApplyRule, false))
        return false;
      continue;
    }

    ASSERT(range.peek().getBlockType() != CSSParserToken::BlockEnd);

    const CSSParserToken& token = range.consume();
    switch (token.type()) {
      case AtKeywordToken: {
        if (equalIgnoringASCIICase(token.value(), "apply")) {
          range.consumeWhitespace();
          const CSSParserToken& variableName =
              range.consumeIncludingWhitespace();
          if (!CSSVariableParser::isValidVariableName(variableName) ||
              !(range.atEnd() || range.peek().type() == SemicolonToken ||
                range.peek().type() == RightBraceToken))
            return false;
          hasAtApplyRule = true;
        }
        break;
      }
      case DelimiterToken: {
        if (token.delimiter() == '!' && isTopLevelBlock)
          return false;
        break;
      }
      case RightParenthesisToken:
      case RightBraceToken:
      case RightBracketToken:
      case BadStringToken:
      case BadUrlToken:
        return false;
      case SemicolonToken:
        if (isTopLevelBlock)
          return false;
        break;
      default:
        break;
    }
  }
  return true;
}

bool isValidVariableReference(CSSParserTokenRange range, bool& hasAtApplyRule) {
  range.consumeWhitespace();
  if (!CSSVariableParser::isValidVariableName(
          range.consumeIncludingWhitespace()))
    return false;
  if (range.atEnd())
    return true;

  if (range.consume().type() != CommaToken)
    return false;
  if (range.atEnd())
    return false;

  bool hasReferences = false;
  return classifyBlock(range, hasReferences, hasAtApplyRule);
}

static CSSValueID classifyVariableRange(CSSParserTokenRange range,
                                        bool& hasReferences,
                                        bool& hasAtApplyRule) {
  hasReferences = false;
  hasAtApplyRule = false;

  range.consumeWhitespace();
  if (range.peek().type() == IdentToken) {
    CSSValueID id = range.consumeIncludingWhitespace().id();
    if (range.atEnd() &&
        (id == CSSValueInherit || id == CSSValueInitial || id == CSSValueUnset))
      return id;
  }

  if (classifyBlock(range, hasReferences, hasAtApplyRule))
    return CSSValueInternalVariableValue;
  return CSSValueInvalid;
}

bool CSSVariableParser::containsValidVariableReferences(
    CSSParserTokenRange range) {
  bool hasReferences;
  bool hasAtApplyRule;
  CSSValueID type = classifyVariableRange(range, hasReferences, hasAtApplyRule);
  return type == CSSValueInternalVariableValue && hasReferences &&
         !hasAtApplyRule;
}

CSSCustomPropertyDeclaration* CSSVariableParser::parseDeclarationValue(
    const AtomicString& variableName,
    CSSParserTokenRange range,
    bool isAnimationTainted) {
  if (range.atEnd())
    return nullptr;

  bool hasReferences;
  bool hasAtApplyRule;
  CSSValueID type = classifyVariableRange(range, hasReferences, hasAtApplyRule);

  if (type == CSSValueInvalid)
    return nullptr;
  if (type == CSSValueInternalVariableValue) {
    return CSSCustomPropertyDeclaration::create(
        variableName, CSSVariableData::create(range, isAnimationTainted,
                                              hasReferences || hasAtApplyRule));
  }
  return CSSCustomPropertyDeclaration::create(variableName, type);
}

CSSVariableReferenceValue* CSSVariableParser::parseRegisteredPropertyValue(
    CSSParserTokenRange range,
    bool requireVarReference,
    bool isAnimationTainted) {
  if (range.atEnd())
    return nullptr;

  bool hasReferences;
  bool hasAtApplyRule;
  CSSValueID type = classifyVariableRange(range, hasReferences, hasAtApplyRule);

  if (type != CSSValueInternalVariableValue)
    return nullptr;  // Invalid or a css-wide keyword
  if (requireVarReference && !hasReferences)
    return nullptr;
  // TODO(timloh): Should this be hasReferences || hasAtApplyRule?
  return CSSVariableReferenceValue::create(
      CSSVariableData::create(range, isAnimationTainted, hasReferences));
}

}  // namespace blink
