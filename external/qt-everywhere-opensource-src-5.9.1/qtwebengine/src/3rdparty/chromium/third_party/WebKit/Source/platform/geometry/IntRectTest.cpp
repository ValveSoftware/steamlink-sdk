// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/IntRect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(IntRectTest, ToString) {
  IntRect emptyRect = IntRect();
  EXPECT_EQ("0,0 0x0", emptyRect.toString());

  IntRect rect(1, 2, 3, 4);
  EXPECT_EQ("1,2 3x4", rect.toString());
}

}  // namespace blink
