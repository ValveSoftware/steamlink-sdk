// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/mock_media_codec_bridge.h"

#include "media/base/subsample_entry.h"

using ::testing::_;
using ::testing::Return;

namespace media {

MockMediaCodecBridge::MockMediaCodecBridge() {
  ON_CALL(*this, DequeueInputBuffer(_, _))
      .WillByDefault(Return(MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER));
  ON_CALL(*this, DequeueOutputBuffer(_, _, _, _, _, _, _))
      .WillByDefault(Return(MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER));
}

MockMediaCodecBridge::~MockMediaCodecBridge() {}

}  // namespace media
