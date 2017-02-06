// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_DISPLAY_COMPOSITOR_H_
#define COMPONENTS_MUS_SURFACES_DISPLAY_COMPOSITOR_H_

#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_factory_client.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class Display;
}

namespace mus {

// TODO(fsamuel): This should become a mojo interface for the mus-gpu split.
// TODO(fsamuel): This should not be a SurfaceFactoryClient.
// The DisplayCompositor receives CompositorFrames from all sources,
// creates a top-level CompositorFrame once per tick, and generates graphical
// output.
class DisplayCompositor : public cc::SurfaceFactoryClient,
                          public cc::DisplayClient {
 public:
  DisplayCompositor(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                    gfx::AcceleratedWidget widget,
                    const scoped_refptr<GpuState>& gpu_state,
                    const scoped_refptr<SurfacesState>& surfaces_state);
  ~DisplayCompositor() override;

  // DisplayCompositor embedders submit a CompositorFrame when content on the
  // display should be changed. A well-behaving embedder should only submit
  // a CompositorFrame once per BeginFrame tick. The callback is called the
  // first time this frame is used to draw, or if the frame is discarded.
  void SubmitCompositorFrame(
      cc::CompositorFrame frame,
      const base::Callback<void(cc::SurfaceDrawStatus)>& callback);

  // TODO(fsamuel): This is used for surface hittesting and should not be
  // exposed outside of DisplayCompositor.
  const cc::SurfaceId& surface_id() const { return surface_id_; }

  // This requests the display CompositorFrame be rendered and given to the
  // callback within CopyOutputRequest.
  void RequestCopyOfOutput(
      std::unique_ptr<cc::CopyOutputRequest> output_request);

  // TODO(fsamuel): Invent an async way to create a SurfaceNamespace
  // A SurfaceNamespace can create CompositorFrameSinks where the client can
  // make up the ID.

 private:
  // SurfaceFactoryClient implementation.
  void ReturnResources(const cc::ReturnedResourceArray& resources) override;
  void SetBeginFrameSource(cc::BeginFrameSource* begin_frame_source) override;

  // DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplaySetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<SurfacesState> surfaces_state_;
  cc::SurfaceFactory factory_;
  cc::SurfaceIdAllocator allocator_;
  cc::SurfaceId surface_id_;

  gfx::Size display_size_;
  std::unique_ptr<cc::Display> display_;
  DISALLOW_COPY_AND_ASSIGN(DisplayCompositor);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_DISPLAY_COMPOSITOR_H_
