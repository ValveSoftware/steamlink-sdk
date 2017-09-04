// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/sys_byteorder.h"
#include "blimp/net/common.h"
#include "blimp/net/compressed_packet_reader.h"
#include "blimp/net/compressed_packet_writer.h"
#include "blimp/net/test_common.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::InvokeArgument;
using testing::SaveArg;

namespace blimp {
namespace {

// Calls the CompletionCallback (indicated by the 0-based index cb_idx)
// with the value |result|.
ACTION_TEMPLATE(InvokeCompletionCallback,
                HAS_1_TEMPLATE_PARAMS(int, cb_idx),
                AND_1_VALUE_PARAMS(result)) {
  testing::get<cb_idx>(args).Run(result);
}

// Copies a DrainableIOBuffer to a GrowableIOBuffer.
// |dest_buf_idx|: The 0-based index of a GrowableIOBuffer.
// |src_buf|: A DrainableIOBuffer.
ACTION_TEMPLATE(CopyBuffer,
                HAS_1_TEMPLATE_PARAMS(int, dest_buf_idx),
                AND_1_VALUE_PARAMS(src_buf)) {
  const auto& dest_buf = testing::get<dest_buf_idx>(args);
  ASSERT_TRUE(dest_buf);
  ASSERT_TRUE(src_buf);
  ASSERT_EQ(0, dest_buf->offset());
  src_buf->SetOffset(0);
  dest_buf->SetCapacity(src_buf->BytesRemaining());
  memcpy(dest_buf->data(), src_buf->data(), src_buf->BytesRemaining());
}

class CompressedPacketTest : public testing::Test {
 public:
  CompressedPacketTest()
      : mock_reader_(new MockPacketReader),
        mock_writer_(new MockPacketWriter),
        compressed_reader_(
            new CompressedPacketReader(base::WrapUnique(mock_reader_))),
        compressed_writer_(
            new CompressedPacketWriter(base::WrapUnique(mock_writer_))) {}
  ~CompressedPacketTest() override {}

 protected:
  // Returns the compressed result of |content|.
  std::string Compress(const std::string& content) {
    scoped_refptr<net::StringIOBuffer> content_str_buf(
        new net::StringIOBuffer(content));
    scoped_refptr<net::DrainableIOBuffer> content_buf(
        new net::DrainableIOBuffer(content_str_buf.get(),
                                   content_str_buf->size()));
    scoped_refptr<net::DrainableIOBuffer> compressed_buf;
    EXPECT_CALL(*mock_writer_, WritePacket(_, _))
        .WillOnce(DoAll(SaveArg<0>(&compressed_buf),
                        InvokeCompletionCallback<1>(net::OK)));
    net::TestCompletionCallback completion_cb_1;
    compressed_writer_->WritePacket(content_buf, completion_cb_1.callback());
    EXPECT_EQ(net::OK, completion_cb_1.WaitForResult());
    return std::string(compressed_buf->data(),
                       compressed_buf->BytesRemaining());
  }

  // Returns the decompressed result of |compressed|.
  std::string Decompress(const std::string& compressed) {
    scoped_refptr<net::StringIOBuffer> compressed_str_buf(
        new net::StringIOBuffer(compressed));
    scoped_refptr<net::DrainableIOBuffer> compressed_buf(
        new net::DrainableIOBuffer(compressed_str_buf.get(),
                                   compressed_str_buf->size()));
    scoped_refptr<net::GrowableIOBuffer> decompressed_buf(
        new net::GrowableIOBuffer);
    EXPECT_CALL(*mock_reader_, ReadPacket(_, _))
        .WillOnce(DoAll(
            CopyBuffer<0>(compressed_buf),
            InvokeCompletionCallback<1>(compressed_buf->BytesRemaining())));
    net::TestCompletionCallback completion_cb_2;
    compressed_reader_->ReadPacket(decompressed_buf,
                                   completion_cb_2.callback());
    int size = completion_cb_2.WaitForResult();
    return std::string(decompressed_buf->data(), size);
  }

  bool CheckRoundTrip(const std::string& content) {
    return Decompress(Compress(content)) == content;
  }

  MockPacketReader* mock_reader_;
  MockPacketWriter* mock_writer_;
  std::unique_ptr<CompressedPacketReader> compressed_reader_;
  std::unique_ptr<CompressedPacketWriter> compressed_writer_;
  testing::InSequence s;
};

TEST_F(CompressedPacketTest, Empty) {
  EXPECT_TRUE(CheckRoundTrip("1234"));
  EXPECT_TRUE(CheckRoundTrip(""));
  EXPECT_TRUE(CheckRoundTrip("1234"));
}

TEST_F(CompressedPacketTest, Simple) {
  std::string source = "AAAAAAAAAAAAAAAAAAAAAAAAA";
  std::string compressed = Compress(source);
  std::string decompressed = Decompress(compressed);
  EXPECT_GT(source.size(), compressed.size());
  EXPECT_EQ(source, decompressed);
}

TEST_F(CompressedPacketTest, DisjointSequences) {
  EXPECT_TRUE(CheckRoundTrip("1234"));
  EXPECT_TRUE(CheckRoundTrip("5678"));
  EXPECT_TRUE(CheckRoundTrip("ABCD"));
}

TEST_F(CompressedPacketTest, AdditiveSequences) {
  EXPECT_TRUE(CheckRoundTrip("12"));
  EXPECT_TRUE(CheckRoundTrip("123"));
  EXPECT_TRUE(CheckRoundTrip("1234"));
  EXPECT_TRUE(CheckRoundTrip("12345"));
  EXPECT_TRUE(CheckRoundTrip("123456"));
}

TEST_F(CompressedPacketTest, ReversedSequences) {
  EXPECT_TRUE(CheckRoundTrip("123456"));
  EXPECT_TRUE(CheckRoundTrip("12345"));
  EXPECT_TRUE(CheckRoundTrip("1234"));
  EXPECT_TRUE(CheckRoundTrip("123"));
  EXPECT_TRUE(CheckRoundTrip("12"));
}

TEST_F(CompressedPacketTest, CompressionStateRetainedAcrossCalls) {
  // Ensure that a character sequence is encoded in a more compact manner
  // if it is compressed more than once.
  int size_1 = Compress("1234").size();
  int size_2 = Compress("1234").size();
  EXPECT_GT(size_1, size_2);
}

TEST_F(CompressedPacketTest, LargeInput) {
  std::string big_str(kMaxPacketPayloadSizeBytes, 'A');  // 3MB of A's.
  EXPECT_TRUE(CheckRoundTrip(big_str));
}

TEST_F(CompressedPacketTest, LowCompressionRatio) {
  // This size (2338) was found "in the wild" to repro an issue with output
  // buffer overflows.
  const int data_size = 2338;

  EXPECT_TRUE(CheckRoundTrip(base::RandBytesAsString(data_size)));
  EXPECT_TRUE(CheckRoundTrip(base::RandBytesAsString(data_size)));
}

TEST_F(CompressedPacketTest, DecompressIllegallyLargePayload) {
  // We can't use the compressor to compress an illegally sized payload, however
  // we can concatenate the output of smaller payloads to form an uber-payload.
  std::string huge_block =
      Compress(std::string(kMaxPacketPayloadSizeBytes, 'A')) +
      Compress("1337 payl0ad 0verfl0w 'spl0it");

  scoped_refptr<net::StringIOBuffer> compressed_str_buf(
      new net::StringIOBuffer(huge_block));
  scoped_refptr<net::DrainableIOBuffer> compressed_buf(
      new net::DrainableIOBuffer(compressed_str_buf.get(),
                                 compressed_str_buf->size()));
  scoped_refptr<net::GrowableIOBuffer> decompressed_buf(
      new net::GrowableIOBuffer);
  EXPECT_CALL(*mock_reader_, ReadPacket(_, _))
      .WillOnce(
          DoAll(CopyBuffer<0>(compressed_buf),
                InvokeCompletionCallback<1>(compressed_buf->BytesRemaining())));
  net::TestCompletionCallback completion_cb_2;
  compressed_reader_->ReadPacket(decompressed_buf, completion_cb_2.callback());
  EXPECT_EQ(net::ERR_FILE_TOO_BIG, completion_cb_2.WaitForResult());
}

TEST_F(CompressedPacketTest, CompressIllegallyLargePayload) {
  scoped_refptr<net::IOBuffer> big_buf(
      new net::IOBuffer(kMaxPacketPayloadSizeBytes + 1));
  scoped_refptr<net::DrainableIOBuffer> content_buf(new net::DrainableIOBuffer(
      big_buf.get(), kMaxPacketPayloadSizeBytes + 1));
  net::TestCompletionCallback completion_cb_1;
  compressed_writer_->WritePacket(content_buf, completion_cb_1.callback());
  EXPECT_EQ(net::ERR_FILE_TOO_BIG, completion_cb_1.WaitForResult());
}

TEST_F(CompressedPacketTest, PayloadSmallerThanDeclared) {
  scoped_refptr<net::StringIOBuffer> content_str_buf(
      new net::StringIOBuffer("Just a small payload"));
  scoped_refptr<net::DrainableIOBuffer> content_buf(new net::DrainableIOBuffer(
      content_str_buf.get(), content_str_buf->size()));
  scoped_refptr<net::DrainableIOBuffer> compressed_buf;
  EXPECT_CALL(*mock_writer_, WritePacket(_, _))
      .WillOnce(DoAll(SaveArg<0>(&compressed_buf),
                      InvokeCompletionCallback<1>(net::OK)));
  net::TestCompletionCallback completion_cb_1;
  compressed_writer_->WritePacket(content_buf, completion_cb_1.callback());
  EXPECT_EQ(net::OK, completion_cb_1.WaitForResult());

  // Increase the payload size header significantly.
  *(reinterpret_cast<uint32_t*>(compressed_buf->data())) =
      base::HostToNet32(kMaxPacketPayloadSizeBytes - 1);

  EXPECT_CALL(*mock_reader_, ReadPacket(_, _))
      .WillOnce(
          DoAll(CopyBuffer<0>(compressed_buf),
                InvokeCompletionCallback<1>(compressed_buf->BytesRemaining())));
  net::TestCompletionCallback completion_cb_2;
  compressed_reader_->ReadPacket(make_scoped_refptr(new net::GrowableIOBuffer),
                                 completion_cb_2.callback());
  EXPECT_EQ(net::ERR_UNEXPECTED, completion_cb_2.WaitForResult());
}

}  // namespace
}  // namespace blimp
