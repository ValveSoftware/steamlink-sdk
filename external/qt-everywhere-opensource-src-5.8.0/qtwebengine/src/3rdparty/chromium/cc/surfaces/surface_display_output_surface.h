// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_DISPLAY_OUTPUT_SURFACE_H_
#define CC_SURFACES_SURFACE_DISPLAY_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "cc/output/output_surface.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {
class Display;
class SurfaceIdAllocator;
class SurfaceManager;

// This class is maps a compositor OutputSurface to the surface system's Display
// concept, allowing a compositor client to submit frames for a native root
// window or physical display.
class CC_SURFACES_EXPORT SurfaceDisplayOutputSurface
    : public OutputSurface,
      public SurfaceFactoryClient,
      public NON_EXPORTED_BASE(DisplayClient) {
 public:
  // The underlying Display and SurfaceManager must outlive this class.
  SurfaceDisplayOutputSurface(
      SurfaceManager* surface_manager,
      SurfaceIdAllocator* allocator,
      Display* display,
      scoped_refptr<ContextProvider> context_provider,
      scoped_refptr<ContextProvider> worker_context_provider);
  SurfaceDisplayOutputSurface(
      SurfaceManager* surface_manager,
      SurfaceIdAllocator* allocator,
      Display* display,
      scoped_refptr<VulkanContextProvider> vulkan_context_provider);
  ~SurfaceDisplayOutputSurface() override;

  // OutputSurface implementation.
  void SwapBuffers(CompositorFrame frame) override;
  bool BindToClient(OutputSurfaceClient* client) override;
  void ForceReclaimResources() override;
  void DetachFromClient() override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;

  // SurfaceFactoryClient implementation.
  void ReturnResources(const ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(BeginFrameSource* begin_frame_source) override;

  // DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplaySetMemoryPolicy(const ManagedMemoryPolicy& policy) override;

 private:
  void SwapBuffersComplete(SurfaceDrawStatus drawn);

  // This class is only meant to be used on a single thread.
  base::ThreadChecker thread_checker_;

  SurfaceManager* surface_manager_;
  SurfaceIdAllocator* surface_id_allocator_;
  Display* display_;
  SurfaceFactory factory_;
  SurfaceId delegated_surface_id_;
  gfx::Size last_swap_frame_size_;
  bool output_surface_lost_ = false;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDisplayOutputSurface);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_DISPLAY_OUTPUT_SURFACE_H_
