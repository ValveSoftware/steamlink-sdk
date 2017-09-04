// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_decoder_buffer_converter.h"

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "media/base/decoder_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

uint32_t kDefaultDataPipeCapacityBytes = 1024;

MATCHER_P(MatchesDecoderBuffer, buffer, "") {
  DCHECK(arg);
  return arg->MatchesForTesting(*buffer);
}

class MockReadCB {
 public:
  MOCK_METHOD1(Run, void(scoped_refptr<DecoderBuffer>));
};

class MojoDecoderBufferConverter {
 public:
  MojoDecoderBufferConverter(
      uint32_t data_pipe_capacity_bytes = kDefaultDataPipeCapacityBytes) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = data_pipe_capacity_bytes;
    mojo::DataPipe data_pipe(options);

    writer = base::MakeUnique<MojoDecoderBufferWriter>(
        std::move(data_pipe.producer_handle));
    reader = base::MakeUnique<MojoDecoderBufferReader>(
        std::move(data_pipe.consumer_handle));
  }

  void ConvertAndVerify(const scoped_refptr<DecoderBuffer>& media_buffer) {
    base::RunLoop run_loop;
    MockReadCB mock_cb;
    EXPECT_CALL(mock_cb, Run(MatchesDecoderBuffer(media_buffer)))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

    mojom::DecoderBufferPtr mojo_buffer =
        writer->WriteDecoderBuffer(media_buffer);
    reader->ReadDecoderBuffer(
        std::move(mojo_buffer),
        base::BindOnce(&MockReadCB::Run, base::Unretained(&mock_cb)));
    run_loop.Run();
  }

  std::unique_ptr<MojoDecoderBufferWriter> writer;
  std::unique_ptr<MojoDecoderBufferReader> reader;
};

}  // namespace

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_Normal) {
  base::MessageLoop message_loop;
  const uint8_t kData[] = "hello, world";
  const uint8_t kSideData[] = "sideshow bob";
  const size_t kDataSize = arraysize(kData);
  const size_t kSideDataSize = arraysize(kSideData);

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize,
      reinterpret_cast<const uint8_t*>(&kSideData), kSideDataSize));
  buffer->set_timestamp(base::TimeDelta::FromMilliseconds(123));
  buffer->set_duration(base::TimeDelta::FromMilliseconds(456));
  buffer->set_splice_timestamp(base::TimeDelta::FromMilliseconds(200));
  buffer->set_discard_padding(
      DecoderBuffer::DiscardPadding(base::TimeDelta::FromMilliseconds(5),
                                    base::TimeDelta::FromMilliseconds(6)));

  MojoDecoderBufferConverter converter;
  converter.ConvertAndVerify(buffer);
}

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_EOS) {
  base::MessageLoop message_loop;
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CreateEOSBuffer());

  MojoDecoderBufferConverter converter;
  converter.ConvertAndVerify(buffer);
}

// TODO(xhwang): Investigate whether we can get rid of zero-byte-buffer.
// See http://crbug.com/663438
TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_ZeroByteBuffer) {
  base::MessageLoop message_loop;
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));

  MojoDecoderBufferConverter converter;
  converter.ConvertAndVerify(buffer);
}

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_KeyFrame) {
  base::MessageLoop message_loop;
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = arraysize(kData);

  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize));
  buffer->set_is_key_frame(true);
  EXPECT_TRUE(buffer->is_key_frame());

  MojoDecoderBufferConverter converter;
  converter.ConvertAndVerify(buffer);
}

TEST(MojoDecoderBufferConverterTest, ConvertDecoderBuffer_EncryptedBuffer) {
  base::MessageLoop message_loop;
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
      base::MakeUnique<DecryptConfig>(kKeyId, kIv, subsamples));
  {
    MojoDecoderBufferConverter converter;
    converter.ConvertAndVerify(buffer);
  }

  // Test empty IV. This is used for clear buffer in an encrypted stream.
  buffer->set_decrypt_config(base::MakeUnique<DecryptConfig>(
      kKeyId, "", std::vector<SubsampleEntry>()));
  {
    MojoDecoderBufferConverter converter;
    converter.ConvertAndVerify(buffer);
  }
}

// This test verifies that a DecoderBuffer larger than data-pipe capacity
// can be transmitted properly.
TEST(MojoDecoderBufferConverterTest, Chunked) {
  base::MessageLoop message_loop;
  const uint8_t kData[] = "Lorem ipsum dolor sit amet, consectetur cras amet";
  const size_t kDataSize = arraysize(kData);
  scoped_refptr<DecoderBuffer> buffer =
      DecoderBuffer::CopyFrom(kData, kDataSize);

  MojoDecoderBufferConverter converter(kDataSize / 3);
  converter.ConvertAndVerify(buffer);
}

// This test verifies that MojoDecoderBufferWriter returns NULL if data pipe
// is already closed.
TEST(MojoDecoderBufferConverterTest, ReaderSidePipeError) {
  base::MessageLoop message_loop;
  const uint8_t kData[] = "Hello, world";
  const size_t kDataSize = arraysize(kData);
  scoped_refptr<DecoderBuffer> media_buffer =
      DecoderBuffer::CopyFrom(kData, kDataSize);

  MojoDecoderBufferConverter converter;
  // Before sending the buffer, close the handle on reader side.
  converter.reader.reset();
  mojom::DecoderBufferPtr mojo_buffer =
      converter.writer->WriteDecoderBuffer(media_buffer);
  EXPECT_TRUE(mojo_buffer.is_null());
}

// This test verifies that MojoDecoderBufferReader::ReadCB is called with a
// NULL DecoderBuffer if data pipe is closed during transmission.
TEST(MojoDecoderBufferConverterTest, WriterSidePipeError) {
  base::MessageLoop message_loop;
  const uint8_t kData[] = "Lorem ipsum dolor sit amet, consectetur cras amet";
  const size_t kDataSize = arraysize(kData);
  scoped_refptr<DecoderBuffer> media_buffer =
      DecoderBuffer::CopyFrom(kData, kDataSize);

  // Verify that ReadCB is called with a NULL decoder buffer.
  base::RunLoop run_loop;
  MockReadCB mock_cb;
  EXPECT_CALL(mock_cb, Run(testing::IsNull()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  // Make data pipe with capacity smaller than decoder buffer so that only
  // partial data is written.
  MojoDecoderBufferConverter converter(kDataSize / 2);
  mojom::DecoderBufferPtr mojo_buffer =
      converter.writer->WriteDecoderBuffer(media_buffer);
  converter.reader->ReadDecoderBuffer(
      std::move(mojo_buffer),
      base::BindOnce(&MockReadCB::Run, base::Unretained(&mock_cb)));

  // Before the entire data is transmitted, close the handle on writer side.
  // The reader side will get notified and report the error.
  converter.writer.reset();
  run_loop.Run();
}

}  // namespace media
