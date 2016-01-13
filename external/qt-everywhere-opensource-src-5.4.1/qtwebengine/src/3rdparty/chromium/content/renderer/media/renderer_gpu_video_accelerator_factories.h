// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/content_export.h"
#include "media/filters/gpu_video_accelerator_factories.h"
#include "ui/gfx/size.h"

namespace base {
class WaitableEvent;
}

namespace content {
class ContextProviderCommandBuffer;
class GpuChannelHost;
class WebGraphicsContext3DCommandBufferImpl;

// Glue code to expose functionality needed by media::GpuVideoAccelerator to
// RenderViewImpl.  This class is entirely an implementation detail of
// RenderViewImpl and only has its own header to allow extraction of its
// implementation from render_view_impl.cc which is already far too large.
//
// The RendererGpuVideoAcceleratorFactories can be constructed on any thread,
// but subsequent calls to all public methods of the class must be called from
// the |message_loop_proxy_|, as provided during construction.
class CONTENT_EXPORT RendererGpuVideoAcceleratorFactories
    : public media::GpuVideoAcceleratorFactories {
 public:
  // Takes a ref on |gpu_channel_host| and tests |context| for loss before each
  // use.  Safe to call from any thread.
  static scoped_refptr<RendererGpuVideoAcceleratorFactories> Create(
      GpuChannelHost* gpu_channel_host,
      const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider);

  // media::GpuVideoAcceleratorFactories implementation.
  virtual scoped_ptr<media::VideoDecodeAccelerator>
      CreateVideoDecodeAccelerator() OVERRIDE;
  virtual scoped_ptr<media::VideoEncodeAccelerator>
      CreateVideoEncodeAccelerator() OVERRIDE;
  // Creates textures and produces them into mailboxes. Returns true on success
  // or false on failure.
  virtual bool CreateTextures(int32 count,
                              const gfx::Size& size,
                              std::vector<uint32>* texture_ids,
                              std::vector<gpu::Mailbox>* texture_mailboxes,
                              uint32 texture_target) OVERRIDE;
  virtual void DeleteTexture(uint32 texture_id) OVERRIDE;
  virtual void WaitSyncPoint(uint32 sync_point) OVERRIDE;
  virtual void ReadPixels(uint32 texture_id,
                          const gfx::Rect& visible_rect,
                          const SkBitmap& pixels) OVERRIDE;
  virtual base::SharedMemory* CreateSharedMemory(size_t size) OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<RendererGpuVideoAcceleratorFactories>;
  RendererGpuVideoAcceleratorFactories(
      GpuChannelHost* gpu_channel_host,
      const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy,
      const scoped_refptr<ContextProviderCommandBuffer>& context_provider);
  virtual ~RendererGpuVideoAcceleratorFactories();

  // Helper to bind |context_provider| to the |task_runner_| thread after
  // construction.
  void BindContext();

  // Helper to get a pointer to the WebGraphicsContext3DCommandBufferImpl,
  // if it has not been lost yet.
  WebGraphicsContext3DCommandBufferImpl* GetContext3d();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<GpuChannelHost> gpu_channel_host_;
  scoped_refptr<ContextProviderCommandBuffer> context_provider_;

  // For sending requests to allocate shared memory in the Browser process.
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;

  DISALLOW_COPY_AND_ASSIGN(RendererGpuVideoAcceleratorFactories);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_GPU_VIDEO_ACCELERATOR_FACTORIES_H_
