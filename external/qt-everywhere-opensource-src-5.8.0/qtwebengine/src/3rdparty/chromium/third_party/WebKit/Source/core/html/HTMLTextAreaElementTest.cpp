// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLTextAreaElement.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(HTMLTextAreaElementTest, SanitizeUserInputValue)
{
    UChar kLeadSurrogate = 0xD800;
    EXPECT_EQ("", HTMLTextAreaElement::sanitizeUserInputValue("", 0));
    EXPECT_EQ("", HTMLTextAreaElement::sanitizeUserInputValue("a", 0));
    EXPECT_EQ("", HTMLTextAreaElement::sanitizeUserInputValue("\n", 0));
    StringBuilder builder;
    builder.append(kLeadSurrogate);
    String leadSurrogate = builder.toString();
    EXPECT_EQ("", HTMLTextAreaElement::sanitizeUserInputValue(leadSurrogate, 0));

    EXPECT_EQ("", HTMLTextAreaElement::sanitizeUserInputValue("", 1));
    EXPECT_EQ("", HTMLTextAreaElement::sanitizeUserInputValue(leadSurrogate, 1));
    EXPECT_EQ("a", HTMLTextAreaElement::sanitizeUserInputValue("a", 1));
    EXPECT_EQ("", HTMLTextAreaElement::sanitizeUserInputValue("\n", 1));
    EXPECT_EQ("\n", HTMLTextAreaElement::sanitizeUserInputValue("\n", 2));

    EXPECT_EQ("abc", HTMLTextAreaElement::sanitizeUserInputValue(String("abc") + leadSurrogate, 4));
    EXPECT_EQ("a\nc", HTMLTextAreaElement::sanitizeUserInputValue("a\ncdef", 4));
}

} // namespace blink
