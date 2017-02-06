// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/DoubleRect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

#ifndef NDEBUG
TEST(DoubleRectTest, ToString)
{
    DoubleRect emptyRect = DoubleRect();
    EXPECT_EQ(String("0.000000,0.000000 0.000000x0.000000"), emptyRect.toString());

    DoubleRect rect(1, 2, 3, 4);
    EXPECT_EQ(String("1.000000,2.000000 3.000000x4.000000"), rect.toString());

    DoubleRect granularRect(1.6, 2.7, 3.8, 4.9);
    EXPECT_EQ(String("1.600000,2.700000 3.800000x4.900000"), granularRect.toString());
}
#endif

} // namespace blink
