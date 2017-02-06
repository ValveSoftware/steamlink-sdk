// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/net/blimp_connection_statistics.h"
#include "blimp/net/common.h"
#include "blimp/net/stream_packet_writer.h"
#include "blimp/net/test_common.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::InSequence;
using testing::Mock;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

namespace blimp {
namespace {

class StreamPacketWriterTest : public testing::Test {
 public:
  StreamPacketWriterTest()
      : test_data_(
            new net::DrainableIOBuffer(new net::StringIOBuffer(test_data_str_),
                                       test_data_str_.size())),
        message_writer_(&socket_, &statistics_) {}

 protected:
  const std::string test_data_str_ = "U WOT M8";
  scoped_refptr<net::DrainableIOBuffer> test_data_;

  base::MessageLoop message_loop_;
  MockStreamSocket socket_;
  BlimpConnectionStatistics statistics_;
  StreamPacketWriter message_writer_;
  testing::InSequence mock_sequence_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StreamPacketWriterTest);
};

// Successful write with 1 async header write and 1 async payload write.
TEST_F(StreamPacketWriterTest, TestWriteAsync) {
  net::TestCompletionCallback writer_cb;
  net::CompletionCallback header_cb;
  net::CompletionCallback payload_cb;

  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(DoAll(SaveArg<2>(&header_cb), Return(net::ERR_IO_PENDING)));
  message_writer_.WritePacket(test_data_, writer_cb.callback());
  EXPECT_CALL(socket_,
              Write(BufferEquals(test_data_str_), test_data_str_.size(), _))
      .WillOnce(DoAll(SaveArg<2>(&payload_cb), Return(net::ERR_IO_PENDING)));

  header_cb.Run(kPacketHeaderSizeBytes);
  payload_cb.Run(test_data_str_.size());
  EXPECT_EQ(net::OK, writer_cb.WaitForResult());
}

// Successful write with 2 async header writes and 2 async payload writes.
TEST_F(StreamPacketWriterTest, TestPartialWriteAsync) {
  net::TestCompletionCallback writer_cb;
  net::CompletionCallback header_cb;
  net::CompletionCallback payload_cb;

  std::string header = EncodeHeader(test_data_str_.size());
  std::string payload = test_data_str_;

  EXPECT_CALL(socket_, Write(BufferEquals(header), kPacketHeaderSizeBytes, _))
      .WillOnce(DoAll(SaveArg<2>(&header_cb), Return(net::ERR_IO_PENDING)))
      .RetiresOnSaturation();
  EXPECT_CALL(socket_, Write(BufferEquals(header.substr(1, header.size())),
                             header.size() - 1, _))
      .WillOnce(DoAll(SaveArg<2>(&header_cb), Return(net::ERR_IO_PENDING)))
      .RetiresOnSaturation();
  EXPECT_CALL(socket_, Write(BufferEquals(payload), payload.size(), _))
      .WillOnce(DoAll(SaveArg<2>(&payload_cb), Return(net::ERR_IO_PENDING)))
      .RetiresOnSaturation();
  EXPECT_CALL(socket_,
              Write(BufferEquals(payload.substr(1, payload.size() - 1)),
                    payload.size() - 1, _))
      .WillOnce(DoAll(SaveArg<2>(&payload_cb), Return(net::ERR_IO_PENDING)))
      .RetiresOnSaturation();

  message_writer_.WritePacket(test_data_, writer_cb.callback());

  EXPECT_EQ(static_cast<int>(payload.size()),
            statistics_.Get(BlimpConnectionStatistics::BYTES_SENT));

  // Header is written - first one byte, then the remainder.
  header_cb.Run(1);
  header_cb.Run(header.size() - 1);

  // Payload is written - first one byte, then the remainder.
  payload_cb.Run(1);
  payload_cb.Run(payload.size() - 1);

  EXPECT_EQ(net::OK, writer_cb.WaitForResult());
}

// Async socket error while writing data.
TEST_F(StreamPacketWriterTest, TestWriteErrorAsync) {
  net::TestCompletionCallback writer_cb;
  net::CompletionCallback header_cb;
  net::CompletionCallback payload_cb;

  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(DoAll(SaveArg<2>(&header_cb), Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(socket_,
              Write(BufferEquals(test_data_str_), test_data_str_.size(), _))
      .WillOnce(DoAll(SaveArg<2>(&payload_cb), Return(net::ERR_IO_PENDING)));

  message_writer_.WritePacket(test_data_, writer_cb.callback());
  header_cb.Run(kPacketHeaderSizeBytes);
  payload_cb.Run(net::ERR_CONNECTION_RESET);

  EXPECT_EQ(net::ERR_CONNECTION_RESET, writer_cb.WaitForResult());
}

// Successful write with 1 sync header write and 1 sync payload write.
TEST_F(StreamPacketWriterTest, TestWriteSync) {
  net::TestCompletionCallback writer_cb;

  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(Return(kPacketHeaderSizeBytes));
  EXPECT_CALL(socket_,
              Write(BufferEquals(test_data_str_), test_data_str_.size(), _))
      .WillOnce(Return(test_data_str_.size()));

  message_writer_.WritePacket(test_data_, writer_cb.callback());
  EXPECT_EQ(net::OK, writer_cb.WaitForResult());
}

// Successful write with 2 sync header writes and 2 sync payload writes.
TEST_F(StreamPacketWriterTest, TestPartialWriteSync) {
  net::TestCompletionCallback writer_cb;

  std::string header = EncodeHeader(test_data_str_.size());
  std::string payload = test_data_str_;

  EXPECT_CALL(socket_, Write(BufferEquals(header), header.size(), _))
      .WillOnce(Return(1));
  EXPECT_CALL(socket_, Write(BufferEquals(header.substr(1, header.size() - 1)),
                             header.size() - 1, _))
      .WillOnce(Return(header.size() - 1));
  EXPECT_CALL(socket_, Write(BufferEquals(payload), payload.size(), _))
      .WillOnce(Return(1));
  EXPECT_CALL(socket_, Write(BufferEquals(payload.substr(1, payload.size())),
                             payload.size() - 1, _))
      .WillOnce(Return(payload.size() - 1));

  message_writer_.WritePacket(test_data_, writer_cb.callback());
  EXPECT_EQ(net::OK, writer_cb.WaitForResult());
}

// Sync socket error while writing header data.
TEST_F(StreamPacketWriterTest, TestWriteHeaderErrorSync) {
  net::TestCompletionCallback writer_cb;

  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(Return(net::ERR_FAILED));

  message_writer_.WritePacket(test_data_, writer_cb.callback());
  EXPECT_EQ(net::ERR_FAILED, writer_cb.WaitForResult());
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE,
            writer_cb.GetResult(net::ERR_EMPTY_RESPONSE));
  EXPECT_FALSE(writer_cb.have_result());
}

// Sync socket error while writing payload data.
TEST_F(StreamPacketWriterTest, TestWritePayloadErrorSync) {
  net::TestCompletionCallback writer_cb;

  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(Return(kPacketHeaderSizeBytes));
  EXPECT_CALL(socket_,
              Write(BufferEquals(test_data_str_), test_data_str_.size(), _))
      .WillOnce(Return(net::ERR_FAILED));

  message_writer_.WritePacket(test_data_, writer_cb.callback());
  EXPECT_EQ(net::ERR_FAILED, writer_cb.WaitForResult());
}

// Verify that asynchronous header write completions don't cause a
// use-after-free error if the writer object is deleted.
TEST_F(StreamPacketWriterTest, DeletedDuringHeaderWrite) {
  net::TestCompletionCallback writer_cb;
  net::CompletionCallback header_cb;
  net::CompletionCallback payload_cb;
  std::unique_ptr<StreamPacketWriter> writer(
      new StreamPacketWriter(&socket_, &statistics_));

  // Write header.
  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(DoAll(SaveArg<2>(&header_cb), Return(net::ERR_IO_PENDING)));
  writer->WritePacket(test_data_, writer_cb.callback());
  Mock::VerifyAndClearExpectations(&socket_);

  // Header write completion callback is invoked after the writer died.
  writer.reset();
  header_cb.Run(kPacketHeaderSizeBytes);
}

// Verify that asynchronous payload write completions don't cause a
// use-after-free error if the writer object is deleted.
TEST_F(StreamPacketWriterTest, DeletedDuringPayloadWrite) {
  net::TestCompletionCallback writer_cb;
  net::CompletionCallback header_cb;
  net::CompletionCallback payload_cb;
  std::unique_ptr<StreamPacketWriter> writer(
      new StreamPacketWriter(&socket_, &statistics_));

  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(DoAll(SaveArg<2>(&header_cb), Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(socket_,
              Write(BufferEquals(test_data_str_), test_data_str_.size(), _))
      .WillOnce(DoAll(SaveArg<2>(&payload_cb), Return(net::ERR_IO_PENDING)));

  writer->WritePacket(test_data_, writer_cb.callback());

  // Header write completes successfully.
  header_cb.Run(kPacketHeaderSizeBytes);

  // Payload write completion callback is invoked after the writer died.
  writer.reset();
  payload_cb.Run(test_data_str_.size());
}

TEST_F(StreamPacketWriterTest, TestWriteHeaderEOFSync) {
  net::TestCompletionCallback writer_cb;

  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(Return(net::OK));
  message_writer_.WritePacket(test_data_, writer_cb.callback());

  EXPECT_EQ(net::ERR_CONNECTION_CLOSED, writer_cb.WaitForResult());
}

TEST_F(StreamPacketWriterTest, TestWritePayloadEOFSync) {
  net::TestCompletionCallback writer_cb;

  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(Return(kPacketHeaderSizeBytes));
  EXPECT_CALL(socket_,
              Write(BufferEquals(test_data_str_), test_data_str_.size(), _))
      .WillOnce(Return(0));
  message_writer_.WritePacket(test_data_, writer_cb.callback());

  EXPECT_EQ(net::ERR_CONNECTION_CLOSED, writer_cb.WaitForResult());
}

TEST_F(StreamPacketWriterTest, TestWriteHeaderEOFAsync) {
  net::TestCompletionCallback writer_cb;
  net::CompletionCallback header_cb;

  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(DoAll(SaveArg<2>(&header_cb), Return(net::ERR_IO_PENDING)));
  message_writer_.WritePacket(test_data_, writer_cb.callback());
  header_cb.Run(0);

  EXPECT_EQ(net::ERR_CONNECTION_CLOSED, writer_cb.WaitForResult());
}

TEST_F(StreamPacketWriterTest, TestWritePayloadEOFAsync) {
  net::TestCompletionCallback writer_cb;
  net::CompletionCallback header_cb;
  net::CompletionCallback payload_cb;

  EXPECT_CALL(socket_, Write(BufferEquals(EncodeHeader(test_data_str_.size())),
                             kPacketHeaderSizeBytes, _))
      .WillOnce(DoAll(SaveArg<2>(&header_cb), Return(net::ERR_IO_PENDING)));
  EXPECT_CALL(socket_,
              Write(BufferEquals(test_data_str_), test_data_str_.size(), _))
      .WillOnce(DoAll(SaveArg<2>(&payload_cb), Return(net::ERR_IO_PENDING)));
  message_writer_.WritePacket(test_data_, writer_cb.callback());
  header_cb.Run(kPacketHeaderSizeBytes);
  payload_cb.Run(0);

  EXPECT_EQ(net::ERR_CONNECTION_CLOSED, writer_cb.WaitForResult());
}

}  // namespace
}  // namespace blimp
