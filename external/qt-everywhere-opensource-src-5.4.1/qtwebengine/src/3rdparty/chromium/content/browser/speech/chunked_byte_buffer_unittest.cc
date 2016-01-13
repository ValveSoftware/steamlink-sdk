// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "content/browser/speech/chunked_byte_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

typedef std::vector<uint8> ByteVector;

TEST(ChunkedByteBufferTest, BasicTest) {
  ChunkedByteBuffer buffer;

  const uint8 kChunks[] = {
      0x00, 0x00, 0x00, 0x04, 0x01, 0x02, 0x03, 0x04,  // Chunk 1: 4 bytes
      0x00, 0x00, 0x00, 0x02, 0x05, 0x06,  // Chunk 2: 2 bytes
      0x00, 0x00, 0x00, 0x01, 0x07  // Chunk 3: 1 bytes
  };

  EXPECT_EQ(0U, buffer.GetTotalLength());
  EXPECT_FALSE(buffer.HasChunks());

  // Append partially chunk 1.
  buffer.Append(kChunks, 2);
  EXPECT_EQ(2U, buffer.GetTotalLength());
  EXPECT_FALSE(buffer.HasChunks());

  // Complete chunk 1.
  buffer.Append(kChunks + 2, 6);
  EXPECT_EQ(8U, buffer.GetTotalLength());
  EXPECT_TRUE(buffer.HasChunks());

  // Append fully chunk 2.
  buffer.Append(kChunks + 8, 6);
  EXPECT_EQ(14U, buffer.GetTotalLength());
  EXPECT_TRUE(buffer.HasChunks());

  // Remove and check chunk 1.
  scoped_ptr<ByteVector> chunk;
  chunk = buffer.PopChunk();
  EXPECT_TRUE(chunk != NULL);
  EXPECT_EQ(4U, chunk->size());
  EXPECT_EQ(0, std::char_traits<uint8>::compare(kChunks + 4,
                                                &(*chunk)[0],
                                                chunk->size()));
  EXPECT_EQ(6U, buffer.GetTotalLength());
  EXPECT_TRUE(buffer.HasChunks());

  // Read and check chunk 2.
  chunk = buffer.PopChunk();
  EXPECT_TRUE(chunk != NULL);
  EXPECT_EQ(2U, chunk->size());
  EXPECT_EQ(0, std::char_traits<uint8>::compare(kChunks + 12,
                                                &(*chunk)[0],
                                                chunk->size()));
  EXPECT_EQ(0U, buffer.GetTotalLength());
  EXPECT_FALSE(buffer.HasChunks());

  // Append fully chunk 3.
  buffer.Append(kChunks + 14, 5);
  EXPECT_EQ(5U, buffer.GetTotalLength());

  // Remove and check chunk 3.
  chunk = buffer.PopChunk();
  EXPECT_TRUE(chunk != NULL);
  EXPECT_EQ(1U, chunk->size());
  EXPECT_EQ((*chunk)[0], kChunks[18]);
  EXPECT_EQ(0U, buffer.GetTotalLength());
  EXPECT_FALSE(buffer.HasChunks());
}

}  // namespace content
