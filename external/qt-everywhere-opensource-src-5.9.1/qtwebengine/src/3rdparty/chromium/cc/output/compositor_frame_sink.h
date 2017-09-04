// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COMPOSITOR_FRAME_SINK_H_
#define CC_OUTPUT_COMPOSITOR_FRAME_SINK_H_

#include <deque>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"
#include "cc/output/overlay_candidate_validator.h"
#include "cc/output/vulkan_context_provider.h"
#include "cc/resources/returned_resource.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/color_space.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace cc {

class CompositorFrame;
class CompositorFrameSinkClient;
class SharedBitmapManager;

// An interface for submitting CompositorFrames to a display compositor
// which will compose frames from multiple CompositorFrameSinks to show
// on screen to the user.
// If a context_provider() is present, frames should be submitted with
// OpenGL resources (created with the context_provider()). If not, then
// SharedBitmap resources should be used.
class CC_EXPORT CompositorFrameSink {
 public:
  struct Capabilities {
    Capabilities() = default;

    // Whether ForceReclaimResources can be called to reclaim all resources
    // from the CompositorFrameSink.
    bool can_force_reclaim_resources = false;
    // True if sync points for resources are needed when swapping delegated
    // frames.
    bool delegated_sync_points_required = true;
  };

  // Constructor for GL-based and/or software resources.
  // gpu_memory_buffer_manager and shared_bitmap_manager must outlive the
  // CompositorFrameSink.
  // shared_bitmap_manager is optional (won't be used) if context_provider is
  // present.
  // gpu_memory_buffer_manager is optional (won't be used) if context_provider
  // is not present.
  CompositorFrameSink(scoped_refptr<ContextProvider> context_provider,
                      scoped_refptr<ContextProvider> worker_context_provider,
                      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                      SharedBitmapManager* shared_bitmap_manager);

  // Constructor for Vulkan-based resources.
  explicit CompositorFrameSink(
      scoped_refptr<VulkanContextProvider> vulkan_context_provider);

  virtual ~CompositorFrameSink();

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be initialized, since from
  // this point to when DetachFromClient() is called the output surface will
  // only be used on the compositor thread.
  // The caller should call DetachFromClient() on the same thread before
  // destroying the CompositorFrameSink, even if this fails. And BindToClient
  // should not be called twice for a given CompositorFrameSink.
  virtual bool BindToClient(CompositorFrameSinkClient* client);

  // Must be called from the thread where BindToClient was called if
  // BindToClient succeeded, after which the CompositorFrameSink may be
  // destroyed from any thread. This is a place where thread-specific data for
  // the object can be uninitialized.
  virtual void DetachFromClient();

  bool HasClient() { return !!client_; }

  const Capabilities& capabilities() const { return capabilities_; }

  // The ContextProviders may be null if frames should be submitted with
  // software SharedBitmap resources.
  ContextProvider* context_provider() const { return context_provider_.get(); }
  ContextProvider* worker_context_provider() const {
    return worker_context_provider_.get();
  }
  VulkanContextProvider* vulkan_context_provider() const {
    return vulkan_context_provider_.get();
  }
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_;
  }
  SharedBitmapManager* shared_bitmap_manager() const {
    return shared_bitmap_manager_;
  }

  // If supported, this causes a ReclaimResources for all resources that are
  // currently in use.
  virtual void ForceReclaimResources() {}

  // Support for a pull-model where draws are requested by the output surface.
  //
  // CompositorFrameSink::Invalidate is called by the compositor to notify that
  // there's new content.
  virtual void Invalidate() {}

  // For successful swaps, the implementation must call
  // DidReceiveCompositorFrameAck() asynchronously when the frame has been
  // processed in order to unthrottle the next frame.
  virtual void SubmitCompositorFrame(CompositorFrame frame) = 0;

 protected:
  // Bound to the ContextProvider to hear about when it is lost and inform the
  // |client_|.
  void DidLoseCompositorFrameSink();

  CompositorFrameSinkClient* client_ = nullptr;

  struct CompositorFrameSink::Capabilities capabilities_;
  scoped_refptr<ContextProvider> context_provider_;
  scoped_refptr<ContextProvider> worker_context_provider_;
  scoped_refptr<VulkanContextProvider> vulkan_context_provider_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  SharedBitmapManager* shared_bitmap_manager_;
  base::ThreadChecker client_thread_checker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorFrameSink);
};

}  // namespace cc

#endif  // CC_OUTPUT_COMPOSITOR_FRAME_SINK_H_
