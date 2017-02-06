// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define MEDIA_RENDERERS_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/video/video_decode_accelerator.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class SharedMemory;
}

namespace media {

class MockGpuVideoAcceleratorFactories : public GpuVideoAcceleratorFactories {
 public:
  explicit MockGpuVideoAcceleratorFactories(gpu::gles2::GLES2Interface* gles2);
  ~MockGpuVideoAcceleratorFactories() override;

  bool IsGpuVideoAcceleratorEnabled() override;
  // CreateVideo{Decode,Encode}Accelerator returns scoped_ptr, which the mocking
  // framework does not want.  Trampoline them.
  MOCK_METHOD0(DoCreateVideoDecodeAccelerator, VideoDecodeAccelerator*());
  MOCK_METHOD0(DoCreateVideoEncodeAccelerator, VideoEncodeAccelerator*());

  MOCK_METHOD5(CreateTextures,
               bool(int32_t count,
                    const gfx::Size& size,
                    std::vector<uint32_t>* texture_ids,
                    std::vector<gpu::Mailbox>* texture_mailboxes,
                    uint32_t texture_target));
  MOCK_METHOD1(DeleteTexture, void(uint32_t texture_id));
  MOCK_METHOD1(WaitSyncToken, void(const gpu::SyncToken& sync_token));
  MOCK_METHOD0(GetTaskRunner, scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(GetVideoDecodeAcceleratorCapabilities,
               VideoDecodeAccelerator::Capabilities());
  MOCK_METHOD0(GetVideoEncodeAcceleratorSupportedProfiles,
               VideoEncodeAccelerator::SupportedProfiles());

  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;

  bool ShouldUseGpuMemoryBuffersForVideoFrames() const override;
  unsigned ImageTextureTarget(gfx::BufferFormat format) override;
  VideoPixelFormat VideoFrameOutputFormat() override {
    return video_frame_output_format_;
  };

  std::unique_ptr<GpuVideoAcceleratorFactories::ScopedGLContextLock>
  GetGLContextLock() override;

  void SetVideoFrameOutputFormat(
      const VideoPixelFormat video_frame_output_format) {
    video_frame_output_format_ = video_frame_output_format;
  };

  void SetFailToAllocateGpuMemoryBufferForTesting(bool fail) {
    fail_to_allocate_gpu_memory_buffer_ = fail;
  }

  void SetGpuMemoryBuffersInUseByMacOSWindowServer(bool in_use);

  std::unique_ptr<base::SharedMemory> CreateSharedMemory(size_t size) override;

  std::unique_ptr<VideoDecodeAccelerator> CreateVideoDecodeAccelerator()
      override;

  std::unique_ptr<VideoEncodeAccelerator> CreateVideoEncodeAccelerator()
      override;

  gpu::gles2::GLES2Interface* GetGLES2Interface() { return gles2_; }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGpuVideoAcceleratorFactories);

  base::Lock lock_;
  VideoPixelFormat video_frame_output_format_ = PIXEL_FORMAT_I420;

  bool fail_to_allocate_gpu_memory_buffer_ = false;

  gpu::gles2::GLES2Interface* gles2_;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_MOCK_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
