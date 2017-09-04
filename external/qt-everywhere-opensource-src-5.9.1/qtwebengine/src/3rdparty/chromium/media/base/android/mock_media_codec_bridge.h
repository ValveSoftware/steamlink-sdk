// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MOCK_MEDIA_CODEC_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MOCK_MEDIA_CODEC_BRIDGE_H_

#include "media/base/android/media_codec_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MockMediaCodecBridge : public MediaCodecBridge {
 public:
  MockMediaCodecBridge();
  ~MockMediaCodecBridge();

  MOCK_METHOD0(Start, bool());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Flush, MediaCodecStatus());
  MOCK_METHOD1(GetOutputSize, MediaCodecStatus(gfx::Size*));
  MOCK_METHOD1(GetOutputSamplingRate, MediaCodecStatus(int*));
  MOCK_METHOD1(GetOutputChannelCount, MediaCodecStatus(int*));
  MOCK_METHOD4(QueueInputBuffer,
               MediaCodecStatus(int, const uint8_t*, size_t, base::TimeDelta));
  MOCK_METHOD7(QueueSecureInputBuffer,
               MediaCodecStatus(int,
                                const uint8_t*,
                                size_t,
                                const std::string&,
                                const std::string&,
                                const std::vector<SubsampleEntry>&,
                                base::TimeDelta));
  MOCK_METHOD8(QueueSecureInputBuffer,
               MediaCodecStatus(int,
                                const uint8_t*,
                                size_t,
                                const std::vector<char>&,
                                const std::vector<char>&,
                                const SubsampleEntry*,
                                int,
                                base::TimeDelta));
  MOCK_METHOD1(QueueEOS, void(int));
  MOCK_METHOD2(DequeueInputBuffer, MediaCodecStatus(base::TimeDelta, int*));
  MOCK_METHOD7(DequeueOutputBuffer,
               MediaCodecStatus(base::TimeDelta,
                                int*,
                                size_t*,
                                size_t*,
                                base::TimeDelta*,
                                bool*,
                                bool*));
  MOCK_METHOD2(ReleaseOutputBuffer, void(int, bool));
  MOCK_METHOD3(GetInputBuffer, MediaCodecStatus(int, uint8_t**, size_t*));
  MOCK_METHOD4(GetOutputBufferAddress,
               MediaCodecStatus(int, size_t, const uint8_t**, size_t*));
  MOCK_METHOD4(CopyFromOutputBuffer,
               MediaCodecStatus(int, size_t, void*, size_t));
  MOCK_METHOD0(GetName, std::string());
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MOCK_MEDIA_CODEC_BRIDGE_H_
