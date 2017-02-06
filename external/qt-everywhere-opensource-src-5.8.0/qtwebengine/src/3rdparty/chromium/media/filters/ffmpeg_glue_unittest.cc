// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_glue.h"

#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace media {

class MockProtocol : public FFmpegURLProtocol {
 public:
  MockProtocol() {}

  MOCK_METHOD2(Read, int(int size, uint8_t* data));
  MOCK_METHOD1(GetPosition, bool(int64_t* position_out));
  MOCK_METHOD1(SetPosition, bool(int64_t position));
  MOCK_METHOD1(GetSize, bool(int64_t* size_out));
  MOCK_METHOD0(IsStreaming, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockProtocol);
};

class FFmpegGlueTest : public ::testing::Test {
 public:
  FFmpegGlueTest()
      : protocol_(new StrictMock<MockProtocol>()) {
    // IsStreaming() is called when opening.
    EXPECT_CALL(*protocol_.get(), IsStreaming()).WillOnce(Return(true));
    glue_.reset(new FFmpegGlue(protocol_.get()));
    CHECK(glue_->format_context());
    CHECK(glue_->format_context()->pb);
  }

  ~FFmpegGlueTest() override {
    // Ensure |glue_| and |protocol_| are still alive.
    CHECK(glue_.get());
    CHECK(protocol_.get());

    // |protocol_| should outlive |glue_|, so ensure it's destructed first.
    glue_.reset();
  }

  int ReadPacket(int size, uint8_t* data) {
    return glue_->format_context()->pb->read_packet(
        protocol_.get(), data, size);
  }

  int64_t Seek(int64_t offset, int whence) {
    return glue_->format_context()->pb->seek(protocol_.get(), offset, whence);
  }

 protected:
  std::unique_ptr<FFmpegGlue> glue_;
  std::unique_ptr<StrictMock<MockProtocol>> protocol_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegGlueTest);
};

class FFmpegGlueDestructionTest : public ::testing::Test {
 public:
  FFmpegGlueDestructionTest() {}

  void Initialize(const char* filename) {
    data_ = ReadTestDataFile(filename);
    protocol_.reset(new InMemoryUrlProtocol(
        data_->data(), data_->data_size(), false));
    glue_.reset(new FFmpegGlue(protocol_.get()));
    CHECK(glue_->format_context());
    CHECK(glue_->format_context()->pb);
  }

  ~FFmpegGlueDestructionTest() override {
    // Ensure Initialize() was called.
    CHECK(glue_.get());
    CHECK(protocol_.get());

    // |glue_| should be destroyed before |protocol_|.
    glue_.reset();

    // |protocol_| should be destroyed before |data_|.
    protocol_.reset();
    data_ = NULL;
  }

 protected:
  std::unique_ptr<FFmpegGlue> glue_;

 private:
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  scoped_refptr<DecoderBuffer> data_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegGlueDestructionTest);
};

// Ensure writing has been disabled.
TEST_F(FFmpegGlueTest, Write) {
  ASSERT_FALSE(glue_->format_context()->pb->write_packet);
  ASSERT_FALSE(glue_->format_context()->pb->write_flag);
}

// Test both successful and unsuccessful reads pass through correctly.
TEST_F(FFmpegGlueTest, Read) {
  const int kBufferSize = 16;
  uint8_t buffer[kBufferSize];

  // Reads are for the most part straight-through calls to Read().
  InSequence s;
  EXPECT_CALL(*protocol_, Read(0, buffer))
      .WillOnce(Return(0));
  EXPECT_CALL(*protocol_, Read(kBufferSize, buffer))
      .WillOnce(Return(kBufferSize));
  EXPECT_CALL(*protocol_, Read(kBufferSize, buffer))
      .WillOnce(Return(DataSource::kReadError));

  EXPECT_EQ(0, ReadPacket(0, buffer));
  EXPECT_EQ(kBufferSize, ReadPacket(kBufferSize, buffer));
  EXPECT_EQ(AVERROR(EIO), ReadPacket(kBufferSize, buffer));
}

// Test a variety of seek operations.
TEST_F(FFmpegGlueTest, Seek) {
  // SEEK_SET should be a straight-through call to SetPosition(), which when
  // successful will return the result from GetPosition().
  InSequence s;
  EXPECT_CALL(*protocol_, SetPosition(-16))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, SetPosition(16))
      .WillOnce(Return(true));
  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));

  EXPECT_EQ(AVERROR(EIO), Seek(-16, SEEK_SET));
  EXPECT_EQ(8, Seek(16, SEEK_SET));

  // SEEK_CUR should call GetPosition() first, and if it succeeds add the offset
  // to the result then call SetPosition()+GetPosition().
  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));
  EXPECT_CALL(*protocol_, SetPosition(16))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));
  EXPECT_CALL(*protocol_, SetPosition(16))
      .WillOnce(Return(true));
  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));

  EXPECT_EQ(AVERROR(EIO), Seek(8, SEEK_CUR));
  EXPECT_EQ(AVERROR(EIO), Seek(8, SEEK_CUR));
  EXPECT_EQ(16, Seek(8, SEEK_CUR));

  // SEEK_END should call GetSize() first, and if it succeeds add the offset
  // to the result then call SetPosition()+GetPosition().
  EXPECT_CALL(*protocol_, GetSize(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));
  EXPECT_CALL(*protocol_, SetPosition(8))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));
  EXPECT_CALL(*protocol_, SetPosition(8))
      .WillOnce(Return(true));
  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(8), Return(true)));

  EXPECT_EQ(AVERROR(EIO), Seek(-8, SEEK_END));
  EXPECT_EQ(AVERROR(EIO), Seek(-8, SEEK_END));
  EXPECT_EQ(8, Seek(-8, SEEK_END));

  // AVSEEK_SIZE should be a straight-through call to GetSize().
  EXPECT_CALL(*protocol_, GetSize(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, GetSize(_))
      .WillOnce(DoAll(SetArgumentPointee<0>(16), Return(true)));

  EXPECT_EQ(AVERROR(EIO), Seek(0, AVSEEK_SIZE));
  EXPECT_EQ(16, Seek(0, AVSEEK_SIZE));
}

// Ensure destruction release the appropriate resources when OpenContext() is
// never called.
TEST_F(FFmpegGlueDestructionTest, WithoutOpen) {
  Initialize("ten_byte_file");
}

// Ensure destruction releases the appropriate resources when
// avformat_open_input() fails.
TEST_F(FFmpegGlueDestructionTest, WithOpenFailure) {
  Initialize("ten_byte_file");
  ASSERT_FALSE(glue_->OpenContext());
}

// Ensure destruction release the appropriate resources when OpenContext() is
// called, but no streams have been opened.
TEST_F(FFmpegGlueDestructionTest, WithOpenNoStreams) {
  Initialize("no_streams.webm");
  ASSERT_TRUE(glue_->OpenContext());
}

// Ensure destruction release the appropriate resources when OpenContext() is
// called and streams exist.
TEST_F(FFmpegGlueDestructionTest, WithOpenWithStreams) {
  Initialize("bear-320x240.webm");
  ASSERT_TRUE(glue_->OpenContext());
}

// Ensure destruction release the appropriate resources when OpenContext() is
// called and streams have been opened.
TEST_F(FFmpegGlueDestructionTest, WithOpenWithOpenStreams) {
  Initialize("bear-320x240.webm");
  ASSERT_TRUE(glue_->OpenContext());
  ASSERT_GT(glue_->format_context()->nb_streams, 0u);

  // Pick the audio stream (1) so this works when the ffmpeg video decoders are
  // disabled.
  AVCodecContext* context = glue_->format_context()->streams[1]->codec;
  ASSERT_EQ(0, avcodec_open2(
      context, avcodec_find_decoder(context->codec_id), NULL));
}

}  // namespace media
