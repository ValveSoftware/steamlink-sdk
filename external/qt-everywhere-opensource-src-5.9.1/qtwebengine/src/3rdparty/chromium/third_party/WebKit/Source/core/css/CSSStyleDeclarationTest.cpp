// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSStyleDeclaration.h"

#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSTestHelper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSStyleDeclarationTest, getPropertyShorthand) {
  CSSTestHelper helper;

  helper.addCSSRules("div { padding: var(--p); }");
  ASSERT_TRUE(helper.cssRules());
  ASSERT_EQ(1u, helper.cssRules()->length());
  ASSERT_EQ(CSSRule::kStyleRule, helper.cssRules()->item(0)->type());
  CSSStyleRule* styleRule = toCSSStyleRule(helper.cssRules()->item(0));
  CSSStyleDeclaration* style = styleRule->style();
  ASSERT_TRUE(style);
  EXPECT_EQ(AtomicString(), style->getPropertyShorthand("padding"));
}

}  // namespace blink
