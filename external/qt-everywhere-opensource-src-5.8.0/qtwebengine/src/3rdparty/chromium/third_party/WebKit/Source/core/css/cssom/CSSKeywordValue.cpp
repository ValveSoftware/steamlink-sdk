// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSKeywordValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/parser/CSSPropertyParser.h"

namespace blink {

CSSKeywordValue* CSSKeywordValue::create(const String& keyword, ExceptionState& exceptionState)
{
    if (keyword.isEmpty()) {
        exceptionState.throwTypeError("CSSKeywordValue does not support empty strings");
        return nullptr;
    }
    return new CSSKeywordValue(keyword);
}

const String& CSSKeywordValue::keywordValue() const
{
    return m_keywordValue;
}

CSSValueID CSSKeywordValue::keywordValueID() const
{
    return cssValueKeywordID(m_keywordValue);
}

CSSValue* CSSKeywordValue::toCSSValue() const
{
    CSSValueID keywordID = keywordValueID();
    if (keywordID == CSSValueID::CSSValueInvalid) {
        return CSSCustomIdentValue::create(m_keywordValue);
    }
    return CSSPrimitiveValue::createIdentifier(keywordID);
}

} // namespace blink
