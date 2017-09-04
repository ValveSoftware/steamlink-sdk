// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSUnparsedValue.h"

#include "core/css/cssom/CSSStyleVariableReferenceValue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

CSSUnparsedValue* unparsedValueFromCSSVariableReferenceValue(
    CSSStyleVariableReferenceValue* variableReferenceValue) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  fragments.append(
      StringOrCSSVariableReferenceValue::fromCSSVariableReferenceValue(
          variableReferenceValue));
  return CSSUnparsedValue::create(fragments);
}

}  // namespace

TEST(CSSUnparsedValueTest, EmptyList) {
  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  CSSUnparsedValue* unparsedValue = CSSUnparsedValue::create(fragments);

  EXPECT_EQ(unparsedValue->size(), 0UL);
}

TEST(CSSUnparsedValueTest, ListOfStrings) {
  CSSUnparsedValue* unparsedValue = CSSUnparsedValue::fromString("string");

  EXPECT_EQ(unparsedValue->size(), 1UL);

  EXPECT_TRUE(unparsedValue->fragmentAtIndex(0).isString());
  EXPECT_FALSE(unparsedValue->fragmentAtIndex(0).isNull());
  EXPECT_FALSE(unparsedValue->fragmentAtIndex(0).isCSSVariableReferenceValue());

  EXPECT_EQ(unparsedValue->fragmentAtIndex(0).getAsString(), "string");
}

TEST(CSSUnparsedValueTest, ListOfCSSVariableReferenceValues) {
  CSSStyleVariableReferenceValue* variableReferenceValue =
      CSSStyleVariableReferenceValue::create(
          "Ref", CSSUnparsedValue::fromString("string"));

  CSSUnparsedValue* unparsedValue =
      unparsedValueFromCSSVariableReferenceValue(variableReferenceValue);

  EXPECT_EQ(unparsedValue->size(), 1UL);

  EXPECT_FALSE(unparsedValue->fragmentAtIndex(0).isString());
  EXPECT_FALSE(unparsedValue->fragmentAtIndex(0).isNull());
  EXPECT_TRUE(unparsedValue->fragmentAtIndex(0).isCSSVariableReferenceValue());

  EXPECT_EQ(unparsedValue->fragmentAtIndex(0).getAsCSSVariableReferenceValue(),
            variableReferenceValue);
}

TEST(CSSUnparsedValueTest, MixedList) {
  CSSStyleVariableReferenceValue* variableReferenceValue =
      CSSStyleVariableReferenceValue::create(
          "Ref", CSSUnparsedValue::fromString("string"));

  HeapVector<StringOrCSSVariableReferenceValue> fragments;
  fragments.append(StringOrCSSVariableReferenceValue::fromString("string"));
  fragments.append(
      StringOrCSSVariableReferenceValue::fromCSSVariableReferenceValue(
          variableReferenceValue));
  fragments.append(StringOrCSSVariableReferenceValue());

  CSSUnparsedValue* unparsedValue = CSSUnparsedValue::create(fragments);

  EXPECT_EQ(unparsedValue->size(), fragments.size());

  EXPECT_TRUE(unparsedValue->fragmentAtIndex(0).isString());
  EXPECT_FALSE(unparsedValue->fragmentAtIndex(0).isCSSVariableReferenceValue());
  EXPECT_EQ(unparsedValue->fragmentAtIndex(0).getAsString(), "string");

  EXPECT_TRUE(unparsedValue->fragmentAtIndex(1).isCSSVariableReferenceValue());
  EXPECT_FALSE(unparsedValue->fragmentAtIndex(1).isString());
  EXPECT_EQ(unparsedValue->fragmentAtIndex(1).getAsCSSVariableReferenceValue(),
            variableReferenceValue);

  EXPECT_TRUE(unparsedValue->fragmentAtIndex(2).isNull());
}

}  // namespace blink
