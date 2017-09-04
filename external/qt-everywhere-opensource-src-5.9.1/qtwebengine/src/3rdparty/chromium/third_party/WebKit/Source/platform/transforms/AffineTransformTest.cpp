// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/transforms/AffineTransform.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(AffineTransformTest, ToString) {
  AffineTransform identity;
  EXPECT_EQ("identity", identity.toString());
  EXPECT_EQ("[1,0,0,\n0,1,0]", identity.toString(true));

  AffineTransform translation = AffineTransform::translation(7, 9);
  EXPECT_EQ("translation(7,9)", translation.toString());
  EXPECT_EQ("[1,0,7,\n0,1,9]", translation.toString(true));

  AffineTransform rotation;
  rotation.rotate(180);
  EXPECT_EQ("translation(0,0), scale(1,1), angle(180deg), remainder(1,0,0,1)",
            rotation.toString());
  EXPECT_EQ("[-1,-1.22465e-16,0,\n1.22465e-16,-1,0]", rotation.toString(true));

  AffineTransform columnMajorConstructor(1, 4, 2, 5, 3, 6);
  EXPECT_EQ("[1,2,3,\n4,5,6]", columnMajorConstructor.toString(true));
}

}  // namespace blink
