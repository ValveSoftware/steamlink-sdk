// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/mock_gpu_memory_buffer_video_frame_pool.h"

#include "base/bind.h"

namespace media {

void MockGpuMemoryBufferVideoFramePool::MaybeCreateHardwareFrame(
    const scoped_refptr<VideoFrame>& video_frame,
    const FrameReadyCB& frame_ready_cb) {
  frame_ready_cbs_->push_back(base::Bind(frame_ready_cb, video_frame));
}

}  // namespace media
