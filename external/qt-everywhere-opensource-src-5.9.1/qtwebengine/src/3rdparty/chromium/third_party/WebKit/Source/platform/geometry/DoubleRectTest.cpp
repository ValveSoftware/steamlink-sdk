// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/DoubleRect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(DoubleRectTest, ToString) {
  DoubleRect emptyRect = DoubleRect();
  EXPECT_EQ("0,0 0x0", emptyRect.toString());

  DoubleRect rect(1, 2, 3, 4);
  EXPECT_EQ("1,2 3x4", rect.toString());

  DoubleRect granularRect(1.6, 2.7, 3.8, 4.9);
  EXPECT_EQ("1.6,2.7 3.8x4.9", granularRect.toString());
}

}  // namespace blink
