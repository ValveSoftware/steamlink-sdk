// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSKeywordValue_h
#define CSSKeywordValue_h

#include "core/CSSValueKeywords.h"
#include "core/CoreExport.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT CSSKeywordValue final : public CSSStyleValue {
  WTF_MAKE_NONCOPYABLE(CSSKeywordValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CSSKeywordValue* create(const AtomicString& keyword, ExceptionState&);

  StyleValueType type() const override { return KeywordType; }

  const AtomicString& keywordValue() const;
  CSSValueID keywordValueID() const;

  CSSValue* toCSSValue() const override;

 private:
  explicit CSSKeywordValue(const AtomicString& keyword)
      : m_keywordValue(keyword) {}

  AtomicString m_keywordValue;
};

DEFINE_TYPE_CASTS(CSSKeywordValue,
                  CSSStyleValue,
                  value,
                  value->type() == CSSStyleValue::StyleValueType::KeywordType,
                  value.type() == CSSStyleValue::StyleValueType::KeywordType);

}  // namespace blink

#endif
