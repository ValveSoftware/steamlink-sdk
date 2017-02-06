// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/CompactHTMLToken.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CompactHTMLTokenTest, EmptyAttributeValueFromHTMLToken)
{
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

    CompactHTMLToken ctoken(&token, TextPosition());

    const CompactHTMLToken::Attribute* attributeB = ctoken.getAttributeItem(
        QualifiedName(AtomicString(), "b", AtomicString()));
    ASSERT_TRUE(attributeB);
    EXPECT_FALSE(attributeB->value().isNull());
    EXPECT_TRUE(attributeB->value().isEmpty());

    const CompactHTMLToken::Attribute* attributeC = ctoken.getAttributeItem(
        QualifiedName(AtomicString(), "c", AtomicString()));
    ASSERT_TRUE(attributeC);
    EXPECT_FALSE(attributeC->value().isNull());
    EXPECT_TRUE(attributeC->value().isEmpty());

    const CompactHTMLToken::Attribute* attributeD = ctoken.getAttributeItem(
        QualifiedName(AtomicString(), "d", AtomicString()));
    EXPECT_FALSE(attributeD);
}

} // namespace blink
