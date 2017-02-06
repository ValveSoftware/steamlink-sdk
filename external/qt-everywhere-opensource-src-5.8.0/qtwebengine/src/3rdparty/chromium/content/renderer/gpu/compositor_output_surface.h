// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/output_surface.h"
#include "content/renderer/gpu/compositor_forwarding_message_filter.h"
#include "ipc/ipc_sync_message_filter.h"

namespace IPC {
class Message;
}

namespace cc {
class CompositorFrame;
class CompositorFrameAck;
class ContextProvider;
}

namespace content {
class FrameSwapMessageQueue;

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when BindToClient is called.
class CompositorOutputSurface
    : NON_EXPORTED_BASE(public cc::OutputSurface),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  CompositorOutputSurface(
      int32_t routing_id,
      uint32_t output_surface_id,
      scoped_refptr<cc::ContextProvider> context_provider,
      scoped_refptr<cc::ContextProvider> worker_context_provider,
      scoped_refptr<FrameSwapMessageQueue> swap_frame_message_queue);
  CompositorOutputSurface(
      int32_t routing_id,
      uint32_t output_surface_id,
      scoped_refptr<cc::VulkanContextProvider> vulkan_context_provider,
      scoped_refptr<FrameSwapMessageQueue> swap_frame_message_queue);
  ~CompositorOutputSurface() override;

  // cc::OutputSurface implementation.
  bool BindToClient(cc::OutputSurfaceClient* client) override;
  void DetachFromClient() override;
  void SwapBuffers(cc::CompositorFrame frame) override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;

 protected:
  uint32_t output_surface_id_;

 private:
  class CompositorOutputSurfaceProxy :
      public base::RefCountedThreadSafe<CompositorOutputSurfaceProxy> {
   public:
    explicit CompositorOutputSurfaceProxy(
        CompositorOutputSurface* output_surface)
        : output_surface_(output_surface) {}
    void ClearOutputSurface() { output_surface_ = NULL; }
    void OnMessageReceived(const IPC::Message& message) {
      if (output_surface_)
        output_surface_->OnMessageReceived(message);
    }

   private:
    friend class base::RefCountedThreadSafe<CompositorOutputSurfaceProxy>;
    ~CompositorOutputSurfaceProxy() {}
    CompositorOutputSurface* output_surface_;

    DISALLOW_COPY_AND_ASSIGN(CompositorOutputSurfaceProxy);
  };

  void OnMessageReceived(const IPC::Message& message);
  void OnUpdateVSyncParametersFromBrowser(base::TimeTicks timebase,
                                          base::TimeDelta interval);
  void OnSwapAck(uint32_t output_surface_id, const cc::CompositorFrameAck& ack);
  void OnReclaimResources(uint32_t output_surface_id,
                          const cc::CompositorFrameAck& ack);
  bool Send(IPC::Message* message);

  scoped_refptr<CompositorForwardingMessageFilter> output_surface_filter_;
  CompositorForwardingMessageFilter::Handler output_surface_filter_handler_;
  scoped_refptr<CompositorOutputSurfaceProxy> output_surface_proxy_;
  scoped_refptr<IPC::SyncMessageFilter> message_sender_;
  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;
  int routing_id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_OUTPUT_SURFACE_H_
