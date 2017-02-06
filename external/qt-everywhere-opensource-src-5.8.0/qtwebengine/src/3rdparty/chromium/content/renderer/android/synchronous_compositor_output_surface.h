// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/managed_memory_policy.h"
#include "cc/output/output_surface.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/transform.h"

namespace cc {
class ContextProvider;
class CompositorFrameMetadata;
}

namespace IPC {
class Message;
class Sender;
}

namespace content {

class FrameSwapMessageQueue;
class SynchronousCompositorRegistry;
class WebGraphicsContext3DCommandBufferImpl;

class SynchronousCompositorOutputSurfaceClient {
 public:
  virtual void DidActivatePendingTree() = 0;
  virtual void Invalidate() = 0;
  virtual void SwapBuffers(uint32_t output_surface_id,
                           cc::CompositorFrame frame) = 0;

 protected:
  virtual ~SynchronousCompositorOutputSurfaceClient() {}
};

// Specialization of the output surface that adapts it to implement the
// content::SynchronousCompositor public API. This class effects an "inversion
// of control" - enabling drawing to be  orchestrated by the embedding
// layer, instead of driven by the compositor internals - hence it holds two
// 'client' pointers (|client_| in the OutputSurface baseclass and
// |delegate_|) which represent the consumers of the two roles in plays.
// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when BindToClient is called.
class SynchronousCompositorOutputSurface
    : NON_EXPORTED_BASE(public cc::OutputSurface) {
 public:
  SynchronousCompositorOutputSurface(
      scoped_refptr<cc::ContextProvider> context_provider,
      scoped_refptr<cc::ContextProvider> worker_context_provider,
      int routing_id,
      uint32_t output_surface_id,
      SynchronousCompositorRegistry* registry,
      scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue);
  ~SynchronousCompositorOutputSurface() override;

  void SetSyncClient(SynchronousCompositorOutputSurfaceClient* compositor);
  bool OnMessageReceived(const IPC::Message& message);

  // OutputSurface.
  bool BindToClient(cc::OutputSurfaceClient* surface_client) override;
  void DetachFromClient() override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override;
  void SwapBuffers(cc::CompositorFrame frame) override;
  void Invalidate() override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;

  // Partial SynchronousCompositor API implementation.
  void DemandDrawHw(const gfx::Size& surface_size,
                    const gfx::Transform& transform,
                    const gfx::Rect& viewport,
                    const gfx::Rect& clip,
                    const gfx::Rect& viewport_rect_for_tile_priority,
                    const gfx::Transform& transform_for_tile_priority);
  void DemandDrawSw(SkCanvas* canvas);

 private:
  class SoftwareDevice;
  friend class SoftwareDevice;

  void InvokeComposite(const gfx::Transform& transform,
                       const gfx::Rect& viewport,
                       const gfx::Rect& clip,
                       bool hardware_draw);
  bool Send(IPC::Message* message);
  void DidActivatePendingTree();
  void DeliverMessages();
  bool CalledOnValidThread() const;

  void CancelFallbackTick();
  void FallbackTickFired();

  // IPC handlers.
  void SetMemoryPolicy(size_t bytes_limit);
  void OnReclaimResources(uint32_t output_surface_id,
                          const cc::CompositorFrameAck& ack);

  const int routing_id_;
  const uint32_t output_surface_id_;
  SynchronousCompositorRegistry* const registry_;  // Not owned.
  IPC::Sender* const sender_;  // Not owned.
  bool registered_;

  // Not owned.
  SynchronousCompositorOutputSurfaceClient* sync_client_;

  // Only valid (non-NULL) during a DemandDrawSw() call.
  SkCanvas* current_sw_canvas_;

  cc::ManagedMemoryPolicy memory_policy_;
  bool did_swap_;
  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;

  base::CancelableClosure fallback_tick_;
  bool fallback_tick_pending_;
  bool fallback_tick_running_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCompositorOutputSurface);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_OUTPUT_SURFACE_H_
