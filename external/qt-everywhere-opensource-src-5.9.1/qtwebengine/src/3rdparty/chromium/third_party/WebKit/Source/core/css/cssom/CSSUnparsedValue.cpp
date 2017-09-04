// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnparsedValue.h"

#include "core/css/cssom/CSSStyleVariableReferenceValue.h"
#include "core/css/parser/CSSTokenizer.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

namespace {

class UnparsedValueIterationSource final
    : public ValueIterable<StringOrCSSVariableReferenceValue>::IterationSource {
 public:
  explicit UnparsedValueIterationSource(CSSUnparsedValue* unparsedValue)
      : m_unparsedValue(unparsedValue) {}

  bool next(ScriptState*,
            StringOrCSSVariableReferenceValue& value,
            ExceptionState&) override {
    if (m_index >= m_unparsedValue->size())
      return false;
    value = m_unparsedValue->fragmentAtIndex(m_index);
    return true;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_unparsedValue);
    ValueIterable<StringOrCSSVariableReferenceValue>::IterationSource::trace(
        visitor);
  }

 private:
  const Member<CSSUnparsedValue> m_unparsedValue;
};

StringView findVariableName(CSSParserTokenRange& range) {
  range.consumeWhitespace();
  return range.consume().value();
}

StringOrCSSVariableReferenceValue variableReferenceValue(
    const StringView& variableName,
    const HeapVector<StringOrCSSVariableReferenceValue>& fragments) {
  CSSUnparsedValue* unparsedValue;
  if (fragments.size() == 0)
    unparsedValue = nullptr;
  else
    unparsedValue = CSSUnparsedValue::create(fragments);

  CSSStyleVariableReferenceValue* variableReference =
      CSSStyleVariableReferenceValue::create(variableName.toString(),
                                             unparsedValue);
  return StringOrCSSVariableReferenceValue::fromCSSVariableReferenceValue(
      variableReference);
}

HeapVector<StringOrCSSVariableReferenceValue> parserTokenRangeToFragments(
    CSSParserTokenRange range) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  StringBuilder builder;
  while (!range.atEnd()) {
    if (range.peek().functionId() == CSSValueVar) {
      if (!builder.isEmpty()) {
        fragments.append(
            StringOrCSSVariableReferenceValue::fromString(builder.toString()));
        builder.clear();
      }
      CSSParserTokenRange block = range.consumeBlock();
      StringView variableName = findVariableName(block);
      block.consumeWhitespace();
      if (block.peek().type() == CSSParserTokenType::CommaToken)
        block.consume();
      fragments.append(variableReferenceValue(
          variableName, parserTokenRangeToFragments(block)));
    } else {
      range.consume().serialize(builder);
    }
  }
  if (!builder.isEmpty()) {
    fragments.append(
        StringOrCSSVariableReferenceValue::fromString(builder.toString()));
  }
  return fragments;
}

}  // namespace

ValueIterable<StringOrCSSVariableReferenceValue>::IterationSource*
CSSUnparsedValue::startIteration(ScriptState*, ExceptionState&) {
  return new UnparsedValueIterationSource(this);
}

CSSUnparsedValue* CSSUnparsedValue::fromCSSValue(
    const CSSVariableReferenceValue& cssVariableReferenceValue) {
  return CSSUnparsedValue::create(parserTokenRangeToFragments(
      cssVariableReferenceValue.variableDataValue()->tokenRange()));
}

CSSValue* CSSUnparsedValue::toCSSValue() const {
  StringBuilder tokens;

  for (unsigned i = 0; i < m_fragments.size(); i++) {
    if (i) {
      tokens.append("/**/");
    }
    if (m_fragments[i].isString()) {
      tokens.append(m_fragments[i].getAsString());
    } else if (m_fragments[i].isCSSVariableReferenceValue()) {
      tokens.append(
          m_fragments[i].getAsCSSVariableReferenceValue()->variable());
    } else {
      NOTREACHED();
    }
  }

  CSSTokenizer tokenizer(tokens.toString());
  return CSSVariableReferenceValue::create(CSSVariableData::create(
      tokenizer.tokenRange(), false /* isAnimationTainted */,
      true /* needsVariableResolution */));
}

}  // namespace blink
