// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/HTMLParserThread.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(HTMLParserThreadTest, Init)
{
    // The harness has not initialized the parser thread, due to
    // RuntimeEnabledFeatures gating.
    ASSERT_FALSE(HTMLParserThread::shared());
    HTMLParserThread::init();
    ASSERT_TRUE(HTMLParserThread::shared());
    HTMLParserThread::shutdown();
}

TEST(HTMLParserThreadTest, ShutdownStartup)
{
    // The harness has not initialized the parser thread, due to
    // RuntimeEnabledFeatures gating.
    ASSERT_FALSE(HTMLParserThread::shared());
    HTMLParserThread::init();
    ASSERT_TRUE(HTMLParserThread::shared());

    HTMLParserThread::shutdown();
    ASSERT_FALSE(HTMLParserThread::shared());
    HTMLParserThread::init();
    ASSERT_TRUE(HTMLParserThread::shared());
}

} // namespace blink
