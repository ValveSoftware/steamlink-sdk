// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSUnparsedValue_h
#define CSSUnparsedValue_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/StringOrCSSVariableReferenceValue.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "wtf/Vector.h"

namespace blink {

class CORE_EXPORT CSSUnparsedValue final
    : public CSSStyleValue,
      public ValueIterable<StringOrCSSVariableReferenceValue> {
  WTF_MAKE_NONCOPYABLE(CSSUnparsedValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSUnparsedValue* create(
      const HeapVector<StringOrCSSVariableReferenceValue>& fragments) {
    return new CSSUnparsedValue(fragments);
  }

  static CSSUnparsedValue* fromCSSValue(const CSSVariableReferenceValue&);

  CSSValue* toCSSValue() const override;

  StyleValueType type() const override { return UnparsedType; }

  StringOrCSSVariableReferenceValue fragmentAtIndex(int index) const {
    return m_fragments.at(index);
  }

  size_t size() const { return m_fragments.size(); }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_fragments);
    CSSStyleValue::trace(visitor);
  }

 protected:
  CSSUnparsedValue(
      const HeapVector<StringOrCSSVariableReferenceValue>& fragments)
      : CSSStyleValue(), m_fragments(fragments) {}

 private:
  static CSSUnparsedValue* fromString(String string) {
    HeapVector<StringOrCSSVariableReferenceValue> fragments;
    fragments.append(StringOrCSSVariableReferenceValue::fromString(string));
    return create(fragments);
  }

  IterationSource* startIteration(ScriptState*, ExceptionState&) override;

  FRIEND_TEST_ALL_PREFIXES(CSSUnparsedValueTest, ListOfStrings);
  FRIEND_TEST_ALL_PREFIXES(CSSUnparsedValueTest,
                           ListOfCSSVariableReferenceValues);
  FRIEND_TEST_ALL_PREFIXES(CSSUnparsedValueTest, MixedList);
  FRIEND_TEST_ALL_PREFIXES(CSSVariableReferenceValueTest, MixedList);

  HeapVector<StringOrCSSVariableReferenceValue> m_fragments;
};

}  // namespace blink

#endif  // CSSUnparsedValue_h
