// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_GENERATOR_H_
#define SERVICES_UI_WS_FRAME_GENERATOR_H_

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "cc/surfaces/frame_sink_id.h"
#include "cc/surfaces/local_frame_id.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/surfaces/surface_sequence_generator.h"
#include "services/ui/public/interfaces/window_tree_constants.mojom.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/server_window_tracker.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class CompositorFrame;
class RenderPass;
class SurfaceId;
}

namespace gpu {
class GpuChannelHost;
}

namespace ui {

class DisplayCompositor;

namespace ws {

namespace test {
class FrameGeneratorTest;
}

class FrameGeneratorDelegate;
class ServerWindow;

// Responsible for redrawing the display in response to the redraw requests by
// submitting CompositorFrames to the owned CompositorFrameSink.
class FrameGenerator : public ServerWindowTracker,
                       public cc::mojom::MojoCompositorFrameSinkClient {
 public:
  FrameGenerator(FrameGeneratorDelegate* delegate, ServerWindow* root_window);
  ~FrameGenerator() override;

  void OnGpuChannelEstablished(scoped_refptr<gpu::GpuChannelHost> gpu_channel);

  // Schedules a redraw for the provided region.
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget);

 private:
  friend class ui::ws::test::FrameGeneratorTest;

  // cc::mojom::MojoCompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck() override;
  void OnBeginFrame(const cc::BeginFrameArgs& begin_frame_arags) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;

  // Generates the CompositorFrame.
  cc::CompositorFrame GenerateCompositorFrame(const gfx::Rect& output_rect);

  // DrawWindowTree recursively visits ServerWindows, creating a SurfaceDrawQuad
  // for each that lacks one.
  void DrawWindowTree(cc::RenderPass* pass,
                      ServerWindow* window,
                      const gfx::Vector2d& parent_to_root_origin_offset,
                      float opacity);

  // Adds a reference to the current cc::Surface of the provided
  // |window_compositor_frame_sink|. If an existing reference is held with a
  // different LocalFrameId then release that reference first. This is called on
  // each ServerWindowCompositorFrameSink as FrameGenerator walks the window
  // tree to generate a CompositorFrame. This is done to make sure that the
  // window surfaces are retained for the entirety of the time between
  // submission of the top-level frame to drawing the frame to screen.
  // TODO(fsamuel, kylechar): This will go away once we get surface lifetime
  // management.
  void AddOrUpdateSurfaceReference(mojom::CompositorFrameSinkType type,
                                   ServerWindow* window);

  // Releases any retained references for the provided FrameSink.
  // TODO(fsamuel, kylechar): This will go away once we get surface lifetime
  // management.
  void ReleaseFrameSinkReference(const cc::FrameSinkId& frame_sink_id);

  // Releases all retained references to surfaces.
  // TODO(fsamuel, kylechar): This will go away once we get surface lifetime
  // management.
  void ReleaseAllSurfaceReferences();

  ui::DisplayCompositor* GetDisplayCompositor();

  // ServerWindowObserver implementation.
  void OnWindowDestroying(ServerWindow* window) override;

  FrameGeneratorDelegate* delegate_;
  cc::FrameSinkId frame_sink_id_;
  ServerWindow* const root_window_;
  cc::SurfaceSequenceGenerator surface_sequence_generator_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;

  cc::mojom::MojoCompositorFrameSinkPtr compositor_frame_sink_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;

  struct SurfaceDependency {
    cc::LocalFrameId local_frame_id;
    cc::SurfaceSequence sequence;
  };
  std::unordered_map<cc::FrameSinkId, SurfaceDependency, cc::FrameSinkIdHash>
      dependencies_;

  mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient> binding_;

  base::WeakPtrFactory<FrameGenerator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameGenerator);
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_GENERATOR_H_
