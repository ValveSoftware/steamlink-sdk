// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/DisplayItem.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

#ifndef NDEBUG
TEST(DisplayItemTest, DebugStringsExist)
{
    for (int type = 0; type <= DisplayItem::TypeLast; type++) {
        String debugString = DisplayItem::typeAsDebugString(static_cast<DisplayItem::Type>(type));
        EXPECT_FALSE(debugString.isEmpty());
        EXPECT_NE("Unknown", debugString);
    }
}
#endif // NDEBUG

} // namespace
} // namespace blink
