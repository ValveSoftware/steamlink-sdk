// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/server_window_compositor_frame_sink.h"

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/surface_draw_quad.h"
#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"
#include "services/ui/ws/server_window_delegate.h"

namespace ui {
namespace ws {

ServerWindowCompositorFrameSink::ServerWindowCompositorFrameSink(
    ServerWindowCompositorFrameSinkManager* manager,
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client)
    : frame_sink_id_(frame_sink_id),
      manager_(manager),
      surface_factory_(frame_sink_id_,
                       manager_->GetCompositorFrameSinkManager(),
                       this),
      client_(std::move(client)),
      binding_(this, std::move(request)) {
  cc::SurfaceManager* surface_manager =
      manager_->GetCompositorFrameSinkManager();
  surface_manager->RegisterFrameSinkId(frame_sink_id_);
  surface_manager->RegisterSurfaceFactoryClient(frame_sink_id_, this);
}

ServerWindowCompositorFrameSink::~ServerWindowCompositorFrameSink() {
  // SurfaceFactory's destructor will attempt to return resources which will
  // call back into here and access |client_| so we should destroy
  // |surface_factory_|'s resources early on.
  surface_factory_.DestroyAll();
  cc::SurfaceManager* surface_manager =
      manager_->GetCompositorFrameSinkManager();
  surface_manager->UnregisterSurfaceFactoryClient(frame_sink_id_);
  surface_manager->InvalidateFrameSinkId(frame_sink_id_);
}

void ServerWindowCompositorFrameSink::SetNeedsBeginFrame(
    bool needs_begin_frame) {
  // TODO(fsamuel): Implement this.
}

void ServerWindowCompositorFrameSink::SubmitCompositorFrame(
    cc::CompositorFrame frame) {
  gfx::Size frame_size =
      frame.delegated_frame_data->render_pass_list[0]->output_rect.size();
  // If the size of the CompostiorFrame has changed then destroy the existing
  // Surface and create a new one of the appropriate size.
  if (local_frame_id_.is_null() || frame_size != last_submitted_frame_size_) {
    if (!local_frame_id_.is_null())
      surface_factory_.Destroy(local_frame_id_);
    local_frame_id_ = surface_id_allocator_.GenerateId();
    surface_factory_.Create(local_frame_id_);
  }
  surface_factory_.SubmitCompositorFrame(
      local_frame_id_, std::move(frame),
      base::Bind(&ServerWindowCompositorFrameSink::DidReceiveCompositorFrameAck,
                 base::Unretained(this)));
  last_submitted_frame_size_ = frame_size;
  ServerWindow* window = manager_->window_;
  window->delegate()->OnScheduleWindowPaint(window);
}

void ServerWindowCompositorFrameSink::DidReceiveCompositorFrameAck() {
  if (!client_ || !base::MessageLoop::current())
    return;
  client_->DidReceiveCompositorFrameAck();
}

void ServerWindowCompositorFrameSink::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (!client_ || !base::MessageLoop::current())
    return;
  client_->ReclaimResources(resources);
}

void ServerWindowCompositorFrameSink::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(tansell): Implement this.
}

}  // namespace ws
}  // namespace ui
