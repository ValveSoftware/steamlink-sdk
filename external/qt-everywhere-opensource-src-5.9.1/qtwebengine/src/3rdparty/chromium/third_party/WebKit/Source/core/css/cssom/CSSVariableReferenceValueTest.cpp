// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleVariableReferenceValue.h"

#include "core/css/cssom/CSSUnparsedValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSVariableReferenceValueTest, EmptyList) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  CSSUnparsedValue* unparsedValue = CSSUnparsedValue::create(fragments);

  CSSStyleVariableReferenceValue* variableReferenceValue =
      CSSStyleVariableReferenceValue::create("test", unparsedValue);

  EXPECT_EQ(variableReferenceValue->variable(), "test");
  EXPECT_EQ(variableReferenceValue->fallback(), unparsedValue);
}

TEST(CSSVariableReferenceValueTest, MixedList) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  fragments.append(StringOrCSSVariableReferenceValue::fromString("string"));
  fragments.append(
      StringOrCSSVariableReferenceValue::fromCSSVariableReferenceValue(
          CSSStyleVariableReferenceValue::create(
              "Variable", CSSUnparsedValue::fromString("Fallback"))));
  fragments.append(StringOrCSSVariableReferenceValue());

  CSSUnparsedValue* unparsedValue = CSSUnparsedValue::create(fragments);

  CSSStyleVariableReferenceValue* variableReferenceValue =
      CSSStyleVariableReferenceValue::create("test", unparsedValue);

  EXPECT_EQ(variableReferenceValue->variable(), "test");
  EXPECT_EQ(variableReferenceValue->fallback(), unparsedValue);
}

}  // namespace blink
