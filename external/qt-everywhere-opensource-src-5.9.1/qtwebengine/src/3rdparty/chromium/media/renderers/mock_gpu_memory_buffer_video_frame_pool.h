// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_MOCK_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
#define MEDIA_RENDERERS_MOCK_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_

#include "base/callback.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"

namespace media {

class MockGpuMemoryBufferVideoFramePool : public GpuMemoryBufferVideoFramePool {
 public:
  MockGpuMemoryBufferVideoFramePool(std::vector<base::Closure>* frame_ready_cbs)
      : frame_ready_cbs_(frame_ready_cbs) {}
  void MaybeCreateHardwareFrame(const scoped_refptr<VideoFrame>& video_frame,
                                const FrameReadyCB& frame_ready_cb) override;

 private:
  std::vector<base::Closure>* frame_ready_cbs_;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_MOCK_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
