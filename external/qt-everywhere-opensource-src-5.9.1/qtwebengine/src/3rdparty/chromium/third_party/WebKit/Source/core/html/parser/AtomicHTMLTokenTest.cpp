// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/AtomicHTMLToken.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(AtomicHTMLTokenTest, EmptyAttributeValueFromHTMLToken) {
  HTMLToken token;
  token.beginStartTag('a');
  token.addNewAttribute();
  token.beginAttributeName(3);
  token.appendToAttributeName('b');
  token.endAttributeName(4);
  token.addNewAttribute();
  token.beginAttributeName(5);
  token.appendToAttributeName('c');
  token.endAttributeName(6);
  token.beginAttributeValue(8);
  token.endAttributeValue(8);

  AtomicHTMLToken atoken(token);

  const blink::Attribute* attributeB = atoken.getAttributeItem(
      QualifiedName(AtomicString(), "b", AtomicString()));
  ASSERT_TRUE(attributeB);
  EXPECT_FALSE(attributeB->value().isNull());
  EXPECT_TRUE(attributeB->value().isEmpty());

  const blink::Attribute* attributeC = atoken.getAttributeItem(
      QualifiedName(AtomicString(), "c", AtomicString()));
  ASSERT_TRUE(attributeC);
  EXPECT_FALSE(attributeC->value().isNull());
  EXPECT_TRUE(attributeC->value().isEmpty());

  const blink::Attribute* attributeD = atoken.getAttributeItem(
      QualifiedName(AtomicString(), "d", AtomicString()));
  EXPECT_FALSE(attributeD);
}

TEST(AtomicHTMLTokenTest, EmptyAttributeValueFromCompactHTMLToken) {
  HTMLToken token;
  token.beginStartTag('a');
  token.addNewAttribute();
  token.beginAttributeName(3);
  token.appendToAttributeName('b');
  token.endAttributeName(4);
  token.addNewAttribute();
  token.beginAttributeName(5);
  token.appendToAttributeName('c');
  token.endAttributeName(6);
  token.beginAttributeValue(8);
  token.endAttributeValue(8);

  AtomicHTMLToken atoken(CompactHTMLToken(&token, TextPosition()));

  const blink::Attribute* attributeB = atoken.getAttributeItem(
      QualifiedName(AtomicString(), "b", AtomicString()));
  ASSERT_TRUE(attributeB);
  EXPECT_FALSE(attributeB->value().isNull());
  EXPECT_TRUE(attributeB->value().isEmpty());

  const blink::Attribute* attributeC = atoken.getAttributeItem(
      QualifiedName(AtomicString(), "c", AtomicString()));
  ASSERT_TRUE(attributeC);
  EXPECT_FALSE(attributeC->value().isNull());
  EXPECT_TRUE(attributeC->value().isEmpty());

  const blink::Attribute* attributeD = atoken.getAttributeItem(
      QualifiedName(AtomicString(), "d", AtomicString()));
  EXPECT_FALSE(attributeD);
}

}  // namespace blink
