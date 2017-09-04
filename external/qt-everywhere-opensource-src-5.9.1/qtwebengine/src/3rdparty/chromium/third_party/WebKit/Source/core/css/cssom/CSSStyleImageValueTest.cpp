// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleImageValue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class FakeCSSStyleImageValue : public CSSStyleImageValue {
 public:
  FakeCSSStyleImageValue(CSSImageValue* imageValue,
                         bool cachePending,
                         LayoutSize layoutSize)
      : CSSStyleImageValue(imageValue),
        m_cachePending(cachePending),
        m_layoutSize(layoutSize) {}

  bool isCachePending() const override { return m_cachePending; }
  LayoutSize imageLayoutSize() const override { return m_layoutSize; }

  CSSValue* toCSSValue() const override { return nullptr; }
  StyleValueType type() const override { return Unknown; }

 private:
  bool m_cachePending;
  LayoutSize m_layoutSize;
};

}  // namespace

TEST(CSSStyleImageValueTest, PendingCache) {
  FakeCSSStyleImageValue* styleImageValue = new FakeCSSStyleImageValue(
      CSSImageValue::create(""), true, LayoutSize(100, 100));
  bool isNull;
  EXPECT_EQ(styleImageValue->intrinsicWidth(isNull), 0);
  EXPECT_EQ(styleImageValue->intrinsicHeight(isNull), 0);
  EXPECT_EQ(styleImageValue->intrinsicRatio(isNull), 0);
  EXPECT_TRUE(isNull);
}

TEST(CSSStyleImageValueTest, ValidLoadedImage) {
  FakeCSSStyleImageValue* styleImageValue = new FakeCSSStyleImageValue(
      CSSImageValue::create(""), false, LayoutSize(480, 120));
  bool isNull;
  EXPECT_EQ(styleImageValue->intrinsicWidth(isNull), 480);
  EXPECT_EQ(styleImageValue->intrinsicHeight(isNull), 120);
  EXPECT_EQ(styleImageValue->intrinsicRatio(isNull), 4);
  EXPECT_FALSE(isNull);
}

}  // namespace blink
