// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/FloatQuad.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(FloatQuadTest, ToString) {
  FloatQuad quad(FloatPoint(2, 3), FloatPoint(5, 7), FloatPoint(11, 13),
                 FloatPoint(17, 19));
  EXPECT_EQ("2,3; 5,7; 11,13; 17,19", quad.toString());
}

}  // namespace blink
