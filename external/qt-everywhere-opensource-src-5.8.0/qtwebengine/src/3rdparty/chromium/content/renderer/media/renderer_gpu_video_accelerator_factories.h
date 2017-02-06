// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/content_export.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class WaitableEvent;
}

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferManager;
}

namespace content {
class ContextProviderCommandBuffer;

// Glue code to expose functionality needed by media::GpuVideoAccelerator to
// RenderViewImpl.  This class is entirely an implementation detail of
// RenderViewImpl and only has its own header to allow extraction of its
// implementation from render_view_impl.cc which is already far too large.
//
// The RendererGpuVideoAcceleratorFactories can be constructed on any thread,
// but subsequent calls to all public methods of the class must be called from
// the |task_runner_|, as provided during construction.
class CONTENT_EXPORT RendererGpuVideoAcceleratorFactories
    : public media::GpuVideoAcceleratorFactories {
 public:
  // Takes a ref on |gpu_channel_host| and tests |context| for loss before each
  // use.  Safe to call from any thread.
  static std::unique_ptr<RendererGpuVideoAcceleratorFactories> Create(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
      bool enable_gpu_memory_buffer_video_frames,
      std::vector<unsigned> image_texture_targets,
      bool enable_video_accelerator);

  // media::GpuVideoAcceleratorFactories implementation.
  bool IsGpuVideoAcceleratorEnabled() override;
  std::unique_ptr<media::VideoDecodeAccelerator> CreateVideoDecodeAccelerator()
      override;
  std::unique_ptr<media::VideoEncodeAccelerator> CreateVideoEncodeAccelerator()
      override;
  // Creates textures and produces them into mailboxes. Returns true on success
  // or false on failure.
  bool CreateTextures(int32_t count,
                      const gfx::Size& size,
                      std::vector<uint32_t>* texture_ids,
                      std::vector<gpu::Mailbox>* texture_mailboxes,
                      uint32_t texture_target) override;
  void DeleteTexture(uint32_t texture_id) override;
  void WaitSyncToken(const gpu::SyncToken& sync_token) override;

  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;

  bool ShouldUseGpuMemoryBuffersForVideoFrames() const override;
  unsigned ImageTextureTarget(gfx::BufferFormat format) override;
  media::VideoPixelFormat VideoFrameOutputFormat() override;
  std::unique_ptr<media::GpuVideoAcceleratorFactories::ScopedGLContextLock>
  GetGLContextLock() override;
  bool CheckContextLost();
  std::unique_ptr<base::SharedMemory> CreateSharedMemory(size_t size) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override;

  media::VideoDecodeAccelerator::Capabilities
  GetVideoDecodeAcceleratorCapabilities() override;
  std::vector<media::VideoEncodeAccelerator::SupportedProfile>
      GetVideoEncodeAcceleratorSupportedProfiles() override;

  void ReleaseContextProvider();
  scoped_refptr<ContextProviderCommandBuffer> ContextProviderMainThread();

  ~RendererGpuVideoAcceleratorFactories() override;

 private:
  RendererGpuVideoAcceleratorFactories(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          main_thread_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider,
      bool enable_gpu_memory_buffer_video_frames,
      std::vector<unsigned> image_texture_targets,
      bool enable_video_accelerator);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;

  // Shared pointer to a shared context provider that should be accessed
  // and set only on the main thread.
  scoped_refptr<ContextProviderCommandBuffer> context_provider_refptr_;

  // Raw pointer to a context provider accessed from the media thread.
  ContextProviderCommandBuffer* context_provider_;

  // Whether gpu memory buffers should be used to hold video frames data.
  bool enable_gpu_memory_buffer_video_frames_;
  const std::vector<unsigned> image_texture_targets_;
  // Whether video acceleration encoding/decoding should be enabled.
  const bool video_accelerator_enabled_;

  gpu::GpuMemoryBufferManager* const gpu_memory_buffer_manager_;

  // For sending requests to allocate shared memory in the Browser process.
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(RendererGpuVideoAcceleratorFactories);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
