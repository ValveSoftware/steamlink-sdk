// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_
#define SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/output/context_provider.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/surfaces/surfaces_context_provider.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace ui {
namespace ws {

class ServerWindow;
class ServerWindowCompositorFrameSink;
class ServerWindowCompositorFrameSinkManagerTestApi;

struct CompositorFrameSinkData {
  CompositorFrameSinkData();
  CompositorFrameSinkData(CompositorFrameSinkData&& other);
  ~CompositorFrameSinkData();

  CompositorFrameSinkData& operator=(CompositorFrameSinkData&& other);

  cc::SurfaceId latest_submitted_surface_id;
  gfx::Size latest_submitted_frame_size;
  cc::mojom::MojoCompositorFrameSinkPrivatePtr compositor_frame_sink;
  cc::mojom::MojoCompositorFrameSinkPrivateRequest
      pending_compositor_frame_sink_request;
};

// ServerWindowCompositorFrameSinkManager tracks the surfaces associated with a
// ServerWindow.
// TODO(fsamuel): Delete this once window decorations are managed in the window
// manager.
class ServerWindowCompositorFrameSinkManager {
 public:
  explicit ServerWindowCompositorFrameSinkManager(ServerWindow* window);
  ~ServerWindowCompositorFrameSinkManager();

  // Returns true if the CompositorFrameSinks from this manager should be drawn.
  bool ShouldDraw();

  // Creates a new CompositorFrameSink of the specified type, replacing the
  // existing one of the specified type.
  void CreateCompositorFrameSink(
      mojom::CompositorFrameSinkType compositor_frame_sink_type,
      gfx::AcceleratedWidget widget,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      scoped_refptr<SurfacesContextProvider> context_provider,
      cc::mojom::MojoCompositorFrameSinkRequest request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);

  // Adds the provided |frame_sink_id| to this ServerWindow's associated
  // CompositorFrameSink if possible. If this ServerWindow does not have
  // an associated CompositorFrameSink then this method will recursively
  // walk up the window hierarchy and register itself with the first ancestor
  // that has a CompositorFrameSink of the same type. This method returns
  // the FrameSinkId that is the first composited ancestor of the ServerWindow
  // assocaited with the provided |frame_sink_id|.
  void AddChildFrameSinkId(
      mojom::CompositorFrameSinkType compositor_frame_sink_type,
      const cc::FrameSinkId& frame_sink_id);
  void RemoveChildFrameSinkId(
      mojom::CompositorFrameSinkType compositor_frame_sink_type,
      const cc::FrameSinkId& frame_sink_id);

  ServerWindow* window() { return window_; }

  bool HasCompositorFrameSinkOfType(mojom::CompositorFrameSinkType type) const;
  bool HasAnyCompositorFrameSink() const;

  gfx::Size GetLatestFrameSize(mojom::CompositorFrameSinkType type) const;
  cc::SurfaceId GetLatestSurfaceId(mojom::CompositorFrameSinkType type) const;
  void SetLatestSurfaceInfo(mojom::CompositorFrameSinkType type,
                            const cc::SurfaceId& surface_id,
                            const gfx::Size& frame_size);

 private:
  friend class ServerWindowCompositorFrameSinkManagerTestApi;
  friend class ServerWindowCompositorFrameSink;

  // Returns true if a CompositorFrameSink of |type| has been set and has
  // received a frame that is greater than the size of the window.
  bool IsCompositorFrameSinkReadyAndNonEmpty(
      mojom::CompositorFrameSinkType type) const;

  ServerWindow* window_;

  using TypeToCompositorFrameSinkMap =
      std::map<mojom::CompositorFrameSinkType, CompositorFrameSinkData>;

  TypeToCompositorFrameSinkMap type_to_compositor_frame_sink_map_;

  // While true the window is not drawn. This is initially true if the window
  // has the property |kWaitForUnderlay_Property|. This is set to false once
  // the underlay and default surface have been set *and* their size is at
  // least that of the window. Ideally we would wait for sizes to match, but
  // the underlay is not necessarily as big as the window.
  bool waiting_for_initial_frames_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowCompositorFrameSinkManager);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_SERVER_WINDOW_COMPOSITOR_FRAME_SINK_MANAGER_H_
