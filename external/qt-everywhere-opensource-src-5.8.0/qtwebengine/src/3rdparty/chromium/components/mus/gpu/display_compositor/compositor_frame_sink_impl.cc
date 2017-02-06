// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gpu/display_compositor/compositor_frame_sink_impl.h"

#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/surfaces/surface_factory.h"
#include "components/mus/gpu/display_compositor/compositor_frame_sink_delegate.h"

namespace mus {
namespace gpu {

namespace {

void CallCallback(
    const mojom::CompositorFrameSink::SubmitCompositorFrameCallback& callback,
    cc::SurfaceDrawStatus draw_status) {
  callback.Run(static_cast<mojom::CompositorFrameDrawStatus>(draw_status));
}
}

CompositorFrameSinkImpl::CompositorFrameSinkImpl(
    CompositorFrameSinkDelegate* delegate,
    int sink_id,
    const scoped_refptr<SurfacesState>& surfaces_state,
    mojo::InterfaceRequest<mojom::CompositorFrameSink> request,
    mojom::CompositorFrameSinkClientPtr client)
    : delegate_(delegate),
      surfaces_state_(surfaces_state),
      sink_id_(sink_id),
      begin_frame_source_(nullptr),
      needs_begin_frame_(false),
      factory_(surfaces_state->manager(), this),
      client_(std::move(client)),
      binding_(this, std::move(request)) {
  DCHECK(delegate_);
  binding_.set_connection_error_handler(base::Bind(
      &CompositorFrameSinkImpl::OnConnectionLost, base::Unretained(this)));
}

CompositorFrameSinkImpl::~CompositorFrameSinkImpl() {}

void CompositorFrameSinkImpl::SetNeedsBeginFrame(bool needs_begin_frame) {
  if (needs_begin_frame_ == needs_begin_frame)
    return;

  needs_begin_frame_ = needs_begin_frame;
  if (begin_frame_source_) {
    if (needs_begin_frame_)
      begin_frame_source_->AddObserver(this);
    else
      begin_frame_source_->RemoveObserver(this);
  }
}

void CompositorFrameSinkImpl::SubmitCompositorFrame(
    cc::CompositorFrame compositor_frame,
    const SubmitCompositorFrameCallback& callback) {
  gfx::Size frame_size =
      compositor_frame.delegated_frame_data->render_pass_list.back()
          ->output_rect.size();
  if (frame_size.IsEmpty() || frame_size != last_submitted_frame_size_) {
    if (!surface_id_.is_null())
      factory_.Destroy(surface_id_);
    // TODO(fsamuel): The allocator should live on CompositorFrameSinkFactory.
    surface_id_ = delegate_->GenerateSurfaceId();
    factory_.Create(surface_id_);
    last_submitted_frame_size_ = frame_size;
  }
  factory_.SubmitCompositorFrame(surface_id_, std::move(compositor_frame),
                                 base::Bind(&CallCallback, callback));
}

void CompositorFrameSinkImpl::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  if (!client_)
    return;

  client_->ReturnResources(mojo::Array<cc::ReturnedResource>::From(resources));
}

void CompositorFrameSinkImpl::WillDrawSurface(const cc::SurfaceId& surface_id,
                                              const gfx::Rect& damage_rect) {
  NOTIMPLEMENTED();
}

void CompositorFrameSinkImpl::SetBeginFrameSource(
    cc::BeginFrameSource* begin_frame_source) {
  begin_frame_source_ = begin_frame_source;
}

void CompositorFrameSinkImpl::OnBeginFrame(const cc::BeginFrameArgs& args) {
  // TODO(fsamuel): Implement this.
}

const cc::BeginFrameArgs& CompositorFrameSinkImpl::LastUsedBeginFrameArgs()
    const {
  // TODO(fsamuel): Implement this.
  return last_used_begin_frame_args_;
}

void CompositorFrameSinkImpl::OnBeginFrameSourcePausedChanged(bool paused) {
  // TODO(fsamuel): Implement this.
}

void CompositorFrameSinkImpl::OnConnectionLost() {
  delegate_->CompositorFrameSinkConnectionLost(sink_id_);
}

}  // namespace gpu
}  // namespace mus
