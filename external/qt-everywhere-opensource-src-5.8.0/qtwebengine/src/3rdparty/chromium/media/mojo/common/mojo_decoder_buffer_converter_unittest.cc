// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_decoder_buffer_converter.h"

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "media/base/decoder_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

scoped_refptr<DecoderBuffer> ConvertDecoderBufferAndBack(
    scoped_refptr<DecoderBuffer> buffer) {
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  std::unique_ptr<MojoDecoderBufferWriter> writer =
      MojoDecoderBufferWriter::Create(DemuxerStream::AUDIO, &consumer_handle);
  MojoDecoderBufferReader reader(std::move(consumer_handle));

  // Convert from and back.
  mojom::DecoderBufferPtr mojo_buffer = writer->WriteDecoderBuffer(buffer);
  return reader.ReadDecoderBuffer(mojo_buffer);
}

void CompareDecoderBuffer(scoped_refptr<DecoderBuffer> a,
                          scoped_refptr<DecoderBuffer> b) {
  EXPECT_EQ(a->end_of_stream(), b->end_of_stream());
  // According to DecoderBuffer API, it is illegal to call any method when
  // end_of_stream() is true.
  if (a->end_of_stream())
    return;

  EXPECT_EQ(a->timestamp(), b->timestamp());
  EXPECT_EQ(a->duration(), b->duration());
  EXPECT_EQ(a->is_key_frame(), b->is_key_frame());
  EXPECT_EQ(a->splice_timestamp(), b->splice_timestamp());
  EXPECT_EQ(a->discard_padding(), b->discard_padding());

  EXPECT_EQ(a->data_size(), b->data_size());
  if (a->data_size() > 0)
    EXPECT_EQ(0, memcmp(a->data(), b->data(), a->data_size()));

  EXPECT_EQ(a->side_data_size(), b->side_data_size());
  if (a->side_data_size() > 0)
    EXPECT_EQ(0, memcmp(a->side_data(), b->side_data(), a->side_data_size()));

  EXPECT_EQ(a->decrypt_config() == nullptr, b->decrypt_config() == nullptr);
  if (a->decrypt_config())
    EXPECT_TRUE(a->decrypt_config()->Matches(*b->decrypt_config()));
}

}  // namespace

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_Normal) {
  const uint8_t kData[] = "hello, world";
  const uint8_t kSideData[] = "sideshow bob";
  const size_t kDataSize = arraysize(kData);
  const size_t kSideDataSize = arraysize(kSideData);

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize,
      reinterpret_cast<const uint8_t*>(&kSideData), kSideDataSize));
  buffer->set_timestamp(base::TimeDelta::FromMilliseconds(123));
  buffer->set_duration(base::TimeDelta::FromMilliseconds(456));
  buffer->set_splice_timestamp(base::TimeDelta::FromMilliseconds(200));
  buffer->set_discard_padding(
      DecoderBuffer::DiscardPadding(base::TimeDelta::FromMilliseconds(5),
                                    base::TimeDelta::FromMilliseconds(6)));

  scoped_refptr<DecoderBuffer> result = ConvertDecoderBufferAndBack(buffer);
  CompareDecoderBuffer(buffer, result);
}

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_EOS) {
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CreateEOSBuffer());
  scoped_refptr<DecoderBuffer> result = ConvertDecoderBufferAndBack(buffer);
  CompareDecoderBuffer(buffer, result);
}

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_KeyFrame) {
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = arraysize(kData);

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize));
  buffer->set_is_key_frame(true);
  EXPECT_TRUE(buffer->is_key_frame());

  scoped_refptr<DecoderBuffer> result = ConvertDecoderBufferAndBack(buffer);
  CompareDecoderBuffer(buffer, result);
}

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_EncryptedBuffer) {
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = arraysize(kData);
  const char kKeyId[] = "00112233445566778899aabbccddeeff";
  const char kIv[] = "0123456789abcdef";

  std::vector<SubsampleEntry> subsamples;
  subsamples.push_back(SubsampleEntry(10, 20));
  subsamples.push_back(SubsampleEntry(30, 40));
  subsamples.push_back(SubsampleEntry(50, 60));

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize));
  buffer->set_decrypt_config(
      base::WrapUnique(new DecryptConfig(kKeyId, kIv, subsamples)));

  scoped_refptr<DecoderBuffer> result = ConvertDecoderBufferAndBack(buffer);
  CompareDecoderBuffer(buffer, result);

  // Test empty IV. This is used for clear buffer in an encrypted stream.
  buffer->set_decrypt_config(base::WrapUnique(
      new DecryptConfig(kKeyId, "", std::vector<SubsampleEntry>())));
  result = ConvertDecoderBufferAndBack(buffer);
  CompareDecoderBuffer(buffer, result);
}

}  // namespace media
