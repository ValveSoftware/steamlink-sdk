// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/ForwardsTextBuffer.h"

#include "core/editing/EditingTestBase.h"

namespace blink {

class ForwardsTextBufferTest : public EditingTestBase {
};

TEST_F(ForwardsTextBufferTest, pushCharacters)
{
    ForwardsTextBuffer buffer;

    // Basic tests.
    buffer.pushCharacters('a', 1);
    buffer.pushCharacters(1u, 0);
    buffer.pushCharacters('#', 2);
    buffer.pushCharacters('\0', 1);
    EXPECT_EQ('a', buffer[0]);
    EXPECT_EQ('#', buffer[1]);
    EXPECT_EQ('#', buffer[2]);
    EXPECT_EQ('\0', buffer[3]);

    // Tests with buffer reallocation.
    buffer.pushCharacters('A', 4096);
    EXPECT_EQ('a', buffer[0]);
    EXPECT_EQ('#', buffer[1]);
    EXPECT_EQ('#', buffer[2]);
    EXPECT_EQ('\0', buffer[3]);
    EXPECT_EQ('A', buffer[4]);
    EXPECT_EQ('A', buffer[4 + 4095]);
}

TEST_F(ForwardsTextBufferTest, pushRange)
{
    ForwardsTextBuffer buffer;

    // Basic tests.
    buffer.pushRange("ababc", 1);
    buffer.pushRange((UChar*)nullptr, 0);
    buffer.pushRange("#@", 2);
    UChar ch = 'x';
    buffer.pushRange(&ch, 1);
    EXPECT_EQ('a', buffer[0]);
    EXPECT_EQ('#', buffer[1]);
    EXPECT_EQ('@', buffer[2]);
    EXPECT_EQ('x', buffer[3]);

    // Tests with buffer reallocation.
    Vector<UChar> chunk(4096);
    for (unsigned i = 0; i < chunk.size(); ++i)
        chunk[i] = i % 256;
    buffer.pushRange(chunk.data(), chunk.size());
    EXPECT_EQ('a', buffer[0]);
    EXPECT_EQ('#', buffer[1]);
    EXPECT_EQ('@', buffer[2]);
    EXPECT_EQ('x', buffer[3]);
    EXPECT_EQ(0, buffer[4]);
    EXPECT_EQ(1111 % 256, buffer[4 + 1111]);
    EXPECT_EQ(255, buffer[4 + 4095]);
}

} // namespace blink
