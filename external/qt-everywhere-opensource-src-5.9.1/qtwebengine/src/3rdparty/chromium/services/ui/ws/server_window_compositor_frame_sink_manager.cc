// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/gpu_compositor_frame_sink.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_delegate.h"

namespace ui {
namespace ws {

ServerWindowCompositorFrameSinkManager::ServerWindowCompositorFrameSinkManager(
    ServerWindow* window)
    : window_(window),
      waiting_for_initial_frames_(
          window_->properties().count(ui::mojom::kWaitForUnderlay_Property) >
          0) {}

ServerWindowCompositorFrameSinkManager::
    ~ServerWindowCompositorFrameSinkManager() {
}

bool ServerWindowCompositorFrameSinkManager::ShouldDraw() {
  if (!waiting_for_initial_frames_)
    return true;

  waiting_for_initial_frames_ = !IsCompositorFrameSinkReadyAndNonEmpty(
                                    mojom::CompositorFrameSinkType::DEFAULT) ||
                                !IsCompositorFrameSinkReadyAndNonEmpty(
                                    mojom::CompositorFrameSinkType::UNDERLAY);
  return !waiting_for_initial_frames_;
}

void ServerWindowCompositorFrameSinkManager::CreateCompositorFrameSink(
    mojom::CompositorFrameSinkType compositor_frame_sink_type,
    gfx::AcceleratedWidget widget,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    scoped_refptr<SurfacesContextProvider> context_provider,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  cc::FrameSinkId frame_sink_id(
      WindowIdToTransportId(window_->id()),
      static_cast<uint32_t>(compositor_frame_sink_type));
  CompositorFrameSinkData& data =
      type_to_compositor_frame_sink_map_[compositor_frame_sink_type];

  cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request;
  if (data.pending_compositor_frame_sink_request.is_pending()) {
    private_request = std::move(data.pending_compositor_frame_sink_request);
  } else {
    private_request = mojo::GetProxy(&data.compositor_frame_sink);
  }

  // TODO(fsamuel): Create the CompositorFrameSink through the DisplayCompositor
  // mojo interface.
  mojo::MakeStrongBinding(
      base::MakeUnique<GpuCompositorFrameSink>(
          window_->delegate()->GetDisplayCompositor(), frame_sink_id, widget,
          gpu_memory_buffer_manager, std::move(context_provider),
          std::move(request), std::move(client)),
      std::move(private_request));
  if (window_->parent()) {
    window_->delegate()
        ->GetRootWindow(window_)
        ->GetOrCreateCompositorFrameSinkManager()
        ->AddChildFrameSinkId(mojom::CompositorFrameSinkType::DEFAULT,
                              frame_sink_id);
  }
}

void ServerWindowCompositorFrameSinkManager::AddChildFrameSinkId(
    mojom::CompositorFrameSinkType compositor_frame_sink_type,
    const cc::FrameSinkId& frame_sink_id) {
  auto it = type_to_compositor_frame_sink_map_.find(compositor_frame_sink_type);
  if (it != type_to_compositor_frame_sink_map_.end()) {
    it->second.compositor_frame_sink->AddChildFrameSink(frame_sink_id);
    return;
  }
  CompositorFrameSinkData& data =
      type_to_compositor_frame_sink_map_[compositor_frame_sink_type];
  data.pending_compositor_frame_sink_request =
      mojo::GetProxy(&data.compositor_frame_sink);
  data.compositor_frame_sink->AddChildFrameSink(frame_sink_id);
}

void ServerWindowCompositorFrameSinkManager::RemoveChildFrameSinkId(
    mojom::CompositorFrameSinkType compositor_frame_sink_type,
    const cc::FrameSinkId& frame_sink_id) {
  auto it = type_to_compositor_frame_sink_map_.find(compositor_frame_sink_type);
  DCHECK(it != type_to_compositor_frame_sink_map_.end());
  it->second.compositor_frame_sink->AddChildFrameSink(frame_sink_id);
}

bool ServerWindowCompositorFrameSinkManager::HasCompositorFrameSinkOfType(
    mojom::CompositorFrameSinkType type) const {
  return type_to_compositor_frame_sink_map_.count(type) > 0;
}

bool ServerWindowCompositorFrameSinkManager::HasAnyCompositorFrameSink() const {
  return HasCompositorFrameSinkOfType(
             mojom::CompositorFrameSinkType::DEFAULT) ||
         HasCompositorFrameSinkOfType(mojom::CompositorFrameSinkType::UNDERLAY);
}

gfx::Size ServerWindowCompositorFrameSinkManager::GetLatestFrameSize(
    mojom::CompositorFrameSinkType type) const {
  auto it = type_to_compositor_frame_sink_map_.find(type);
  if (it == type_to_compositor_frame_sink_map_.end())
    return gfx::Size();

  return it->second.latest_submitted_frame_size;
}

cc::SurfaceId ServerWindowCompositorFrameSinkManager::GetLatestSurfaceId(
    mojom::CompositorFrameSinkType type) const {
  auto it = type_to_compositor_frame_sink_map_.find(type);
  if (it == type_to_compositor_frame_sink_map_.end())
    return cc::SurfaceId();

  return it->second.latest_submitted_surface_id;
}

void ServerWindowCompositorFrameSinkManager::SetLatestSurfaceInfo(
    mojom::CompositorFrameSinkType type,
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size) {
  CompositorFrameSinkData& data = type_to_compositor_frame_sink_map_[type];
  data.latest_submitted_surface_id = surface_id;
  data.latest_submitted_frame_size = frame_size;
}

bool ServerWindowCompositorFrameSinkManager::
    IsCompositorFrameSinkReadyAndNonEmpty(
        mojom::CompositorFrameSinkType type) const {
  auto iter = type_to_compositor_frame_sink_map_.find(type);
  if (iter == type_to_compositor_frame_sink_map_.end())
    return false;
  if (iter->second.latest_submitted_frame_size.IsEmpty())
    return false;
  const gfx::Size& latest_submitted_frame_size =
      iter->second.latest_submitted_frame_size;
  return latest_submitted_frame_size.width() >= window_->bounds().width() &&
         latest_submitted_frame_size.height() >= window_->bounds().height();
}

CompositorFrameSinkData::CompositorFrameSinkData() {}

CompositorFrameSinkData::~CompositorFrameSinkData() {}

CompositorFrameSinkData::CompositorFrameSinkData(
    CompositorFrameSinkData&& other)
    : latest_submitted_surface_id(other.latest_submitted_surface_id),
      compositor_frame_sink(std::move(other.compositor_frame_sink)) {}

CompositorFrameSinkData& CompositorFrameSinkData::operator=(
    CompositorFrameSinkData&& other) {
  latest_submitted_surface_id = other.latest_submitted_surface_id;
  compositor_frame_sink = std::move(other.compositor_frame_sink);
  return *this;
}

}  // namespace ws
}  // namespace ui
