// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSURLImageValue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

void checkNullURLImageValue(CSSURLImageValue* urlImageValue) {
  EXPECT_EQ(urlImageValue->state(), "unloaded");
  bool isNull;
  EXPECT_EQ(urlImageValue->intrinsicWidth(isNull), 0);
  EXPECT_EQ(urlImageValue->intrinsicHeight(isNull), 0);
  EXPECT_EQ(urlImageValue->intrinsicRatio(isNull), 0);
  EXPECT_TRUE(isNull);
}

}  // namespace

TEST(CSSURLImageValueTest, CreateURLImageValueFromURL) {
  checkNullURLImageValue(CSSURLImageValue::create("http://localhost"));
}

TEST(CSSURLImageValueTest, CreateURLImageValueFromImageValue) {
  checkNullURLImageValue(
      CSSURLImageValue::create(CSSImageValue::create("http://localhost")));
}

}  // namespace blink
