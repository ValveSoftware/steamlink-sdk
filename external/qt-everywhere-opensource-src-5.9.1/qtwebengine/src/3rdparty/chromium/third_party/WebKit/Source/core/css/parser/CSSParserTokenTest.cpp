// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/parser/CSSParserToken.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static CSSParserToken ident(const String& string) {
  return CSSParserToken(IdentToken, string);
}
static CSSParserToken dimension(double value, const String& unit) {
  CSSParserToken token(NumberToken, value, NumberValueType, NoSign);
  token.convertToDimensionWithUnit(unit);
  return token;
}

TEST(CSSParserTokenTest, IdentTokenEquality) {
  String foo8Bit("foo");
  String bar8Bit("bar");
  String foo16Bit =
      String::make16BitFrom8BitSource(foo8Bit.characters8(), foo8Bit.length());

  EXPECT_EQ(ident(foo8Bit), ident(foo16Bit));
  EXPECT_EQ(ident(foo16Bit), ident(foo8Bit));
  EXPECT_EQ(ident(foo16Bit), ident(foo16Bit));
  EXPECT_NE(ident(bar8Bit), ident(foo8Bit));
  EXPECT_NE(ident(bar8Bit), ident(foo16Bit));
}

TEST(CSSParserTokenTest, DimensionTokenEquality) {
  String em8Bit("em");
  String rem8Bit("rem");
  String em16Bit =
      String::make16BitFrom8BitSource(em8Bit.characters8(), em8Bit.length());

  EXPECT_EQ(dimension(1, em8Bit), dimension(1, em16Bit));
  EXPECT_EQ(dimension(1, em8Bit), dimension(1, em8Bit));
  EXPECT_NE(dimension(1, em8Bit), dimension(1, rem8Bit));
  EXPECT_NE(dimension(2, em8Bit), dimension(1, em16Bit));
}

}  // namespace blink
