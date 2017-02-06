// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_codec_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MockMediaCodecLoopClient : public MediaCodecLoop::Client {
 public:
  MOCK_CONST_METHOD0(IsAnyInputPending, bool());
  MOCK_METHOD0(ProvideInputData, MediaCodecLoop::InputData());
  MOCK_METHOD1(OnInputDataQueued, void(bool));
  MOCK_METHOD1(OnDecodedEos, void(const MediaCodecLoop::OutputBuffer&));
  MOCK_METHOD1(OnDecodedFrame, bool(const MediaCodecLoop::OutputBuffer&));
  MOCK_METHOD0(OnOutputFormatChanged, bool());
  MOCK_METHOD0(OnCodecLoopError, void());
};

class MediaCodecLoopTest : public testing::Test {
 public:
  MediaCodecLoopTest() : client_(new MockMediaCodecLoopClient) {}

 public:
  std::unique_ptr<MediaCodecLoop> codec_loop_;
  std::unique_ptr<MockMediaCodecLoopClient> client_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecLoopTest);
};

TEST_F(MediaCodecLoopTest, TestConstructionWithNullCodec) {
  std::unique_ptr<MediaCodecBridge> codec;
  EXPECT_CALL(*client_, OnCodecLoopError()).Times(1);
  codec_loop_.reset(new MediaCodecLoop(client_.get(), std::move(codec)));

  ASSERT_FALSE(codec_loop_->GetCodec());
}

}  // namespace media
