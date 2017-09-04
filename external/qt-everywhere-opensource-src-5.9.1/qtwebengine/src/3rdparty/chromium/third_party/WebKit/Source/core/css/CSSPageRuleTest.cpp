// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSPageRule.h"

#include "core/css/CSSRuleList.h"
#include "core/css/CSSTestHelper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(CSSPageRule, Serializing) {
  CSSTestHelper helper;

  const char* cssRule = "@page :left { size: auto; }";
  helper.addCSSRules(cssRule);
  if (helper.cssRules()) {
    EXPECT_EQ(1u, helper.cssRules()->length());
    EXPECT_EQ(String(cssRule), helper.cssRules()->item(0)->cssText());
    EXPECT_EQ(CSSRule::kPageRule, helper.cssRules()->item(0)->type());
    if (CSSPageRule* pageRule = toCSSPageRule(helper.cssRules()->item(0)))
      EXPECT_EQ(":left", pageRule->selectorText());
  }
}

TEST(CSSPageRule, selectorText) {
  CSSTestHelper helper;

  const char* cssRule = "@page :left { size: auto; }";
  helper.addCSSRules(cssRule);
  ASSERT(helper.cssRules());
  EXPECT_EQ(1u, helper.cssRules()->length());

  CSSPageRule* pageRule = toCSSPageRule(helper.cssRules()->item(0));
  EXPECT_EQ(":left", pageRule->selectorText());

  // set invalid page selector.
  pageRule->setSelectorText(":hover");
  EXPECT_EQ(":left", pageRule->selectorText());

  // set invalid page selector.
  pageRule->setSelectorText("right { bla");
  EXPECT_EQ(":left", pageRule->selectorText());

  // set page pseudo class selector.
  pageRule->setSelectorText(":right");
  EXPECT_EQ(":right", pageRule->selectorText());

  // set page type selector.
  pageRule->setSelectorText("namedpage");
  EXPECT_EQ("namedpage", pageRule->selectorText());
}

}  // namespace blink
