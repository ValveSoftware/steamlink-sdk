// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "media/base/decoder_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(DecoderBufferTest, Constructors) {
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  EXPECT_TRUE(buffer->data());
  EXPECT_EQ(0, buffer->data_size());
  EXPECT_FALSE(buffer->end_of_stream());

  const int kTestSize = 10;
  scoped_refptr<DecoderBuffer> buffer3(new DecoderBuffer(kTestSize));
  ASSERT_TRUE(buffer3.get());
  EXPECT_EQ(kTestSize, buffer3->data_size());
}

TEST(DecoderBufferTest, CreateEOSBuffer) {
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CreateEOSBuffer());
  EXPECT_TRUE(buffer->end_of_stream());
}

TEST(DecoderBufferTest, CopyFrom) {
  const uint8 kData[] = "hello";
  const int kDataSize = arraysize(kData);
  scoped_refptr<DecoderBuffer> buffer2(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(&kData), kDataSize));
  ASSERT_TRUE(buffer2.get());
  EXPECT_NE(kData, buffer2->data());
  EXPECT_EQ(buffer2->data_size(), kDataSize);
  EXPECT_EQ(0, memcmp(buffer2->data(), kData, kDataSize));
  EXPECT_FALSE(buffer2->end_of_stream());
  scoped_refptr<DecoderBuffer> buffer3(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(&kData), kDataSize,
      reinterpret_cast<const uint8*>(&kData), kDataSize));
  ASSERT_TRUE(buffer3.get());
  EXPECT_NE(kData, buffer3->data());
  EXPECT_EQ(buffer3->data_size(), kDataSize);
  EXPECT_EQ(0, memcmp(buffer3->data(), kData, kDataSize));
  EXPECT_NE(kData, buffer3->side_data());
  EXPECT_EQ(buffer3->side_data_size(), kDataSize);
  EXPECT_EQ(0, memcmp(buffer3->side_data(), kData, kDataSize));
  EXPECT_FALSE(buffer3->end_of_stream());
}

#if !defined(OS_ANDROID)
TEST(DecoderBufferTest, PaddingAlignment) {
  const uint8 kData[] = "hello";
  const int kDataSize = arraysize(kData);
  scoped_refptr<DecoderBuffer> buffer2(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(&kData), kDataSize));
  ASSERT_TRUE(buffer2.get());

  // Padding data should always be zeroed.
  for(int i = 0; i < DecoderBuffer::kPaddingSize; i++)
    EXPECT_EQ((buffer2->data() + kDataSize)[i], 0);

  // If the data is padded correctly we should be able to read and write past
  // the end of the data by DecoderBuffer::kPaddingSize bytes without crashing
  // or Valgrind/ASAN throwing errors.
  const uint8 kFillChar = 0xFF;
  memset(
      buffer2->writable_data() + kDataSize, kFillChar,
      DecoderBuffer::kPaddingSize);
  for(int i = 0; i < DecoderBuffer::kPaddingSize; i++)
    EXPECT_EQ((buffer2->data() + kDataSize)[i], kFillChar);

  EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(
      buffer2->data()) & (DecoderBuffer::kAlignmentSize - 1));
}
#endif

TEST(DecoderBufferTest, ReadingWriting) {
  const char kData[] = "hello";
  const int kDataSize = arraysize(kData);

  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(kDataSize));
  ASSERT_TRUE(buffer.get());

  uint8* data = buffer->writable_data();
  ASSERT_TRUE(data);
  ASSERT_EQ(kDataSize, buffer->data_size());
  memcpy(data, kData, kDataSize);
  const uint8* read_only_data = buffer->data();
  ASSERT_EQ(data, read_only_data);
  ASSERT_EQ(0, memcmp(read_only_data, kData, kDataSize));
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST(DecoderBufferTest, GetDecryptConfig) {
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  EXPECT_FALSE(buffer->decrypt_config());
}

}  // namespace media
