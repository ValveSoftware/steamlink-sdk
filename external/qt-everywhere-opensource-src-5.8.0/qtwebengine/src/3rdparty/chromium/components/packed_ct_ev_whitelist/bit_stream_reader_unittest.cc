// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/packed_ct_ev_whitelist/bit_stream_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace packed_ct_ev_whitelist {
namespace internal {

const uint8_t kSomeData[] = {0xd5, 0xe2, 0xaf, 0xe5, 0xbb, 0x10, 0x7c, 0xd1};

TEST(BitStreamReaderTest, CanReadSingleByte) {
  BitStreamReader reader(
      base::StringPiece(reinterpret_cast<const char*>(kSomeData), 1));
  uint64_t v(0);

  EXPECT_EQ(8u, reader.BitsLeft());
  EXPECT_TRUE(reader.ReadBits(8, &v));
  EXPECT_EQ(UINT64_C(0xd5), v);

  EXPECT_FALSE(reader.ReadBits(1, &v));
  EXPECT_EQ(0u, reader.BitsLeft());
}

TEST(BitStreamReaderTest, CanReadSingleBits) {
  const uint64_t expected_bits[] = {
      1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0};
  BitStreamReader reader(
      base::StringPiece(reinterpret_cast<const char*>(kSomeData), 2));
  EXPECT_EQ(16u, reader.BitsLeft());
  uint64_t v(0);

  for (int i = 0; i < 16; ++i) {
    EXPECT_TRUE(reader.ReadBits(1, &v));
    EXPECT_EQ(expected_bits[i], v);
  }
  EXPECT_EQ(0u, reader.BitsLeft());
}

TEST(BitStreamReaderTest, CanReadBitGroups) {
  BitStreamReader reader(
      base::StringPiece(reinterpret_cast<const char*>(kSomeData), 3));
  EXPECT_EQ(24u, reader.BitsLeft());
  uint64_t v(0);
  uint64_t res(0);

  EXPECT_TRUE(reader.ReadBits(5, &v));
  res |= v << 19;
  EXPECT_EQ(19u, reader.BitsLeft());
  EXPECT_TRUE(reader.ReadBits(13, &v));
  res |= v << 6;
  EXPECT_EQ(6u, reader.BitsLeft());
  EXPECT_TRUE(reader.ReadBits(6, &v));
  res |= v;
  EXPECT_EQ(UINT64_C(0xd5e2af), res);

  EXPECT_FALSE(reader.ReadBits(1, &v));
}

TEST(BitStreamReaderTest, CanRead64Bit) {
  BitStreamReader reader(
      base::StringPiece(reinterpret_cast<const char*>(kSomeData), 8));
  EXPECT_EQ(64u, reader.BitsLeft());
  uint64_t v(0);

  EXPECT_TRUE(reader.ReadBits(64, &v));
  EXPECT_EQ(UINT64_C(0xd5e2afe5bb107cd1), v);
}

TEST(BitStreamReaderTest, CanReadUnaryEncodedNumbers) {
  internal::BitStreamReader reader(
      base::StringPiece(reinterpret_cast<const char*>(kSomeData), 3));
  const uint64_t expected_values[] = {2, 1, 1, 4, 0, 0, 1, 1, 1, 4};
  uint64_t v(0);
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(reader.ReadUnaryEncoding(&v));
    EXPECT_EQ(expected_values[i], v) << "Values differ at position " << i;
  }
}

TEST(BitStreamReaderTest, CannotReadFromEmptyStream) {
  BitStreamReader reader(base::StringPiece(
      reinterpret_cast<const char*>(kSomeData), static_cast<size_t>(0u)));
  uint64_t v(0);

  EXPECT_EQ(0u, reader.BitsLeft());
  EXPECT_FALSE(reader.ReadBits(1, &v));
  EXPECT_FALSE(reader.ReadUnaryEncoding(&v));
}

}  // namespace internal
}  // namespace packed_ct_ev_whitelist
