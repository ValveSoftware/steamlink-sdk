// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/frame_generator.h"

#include "base/containers/adapters.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/surfaces/surface_id.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/surfaces/display_compositor.h"
#include "services/ui/surfaces/surfaces_context_provider.h"
#include "services/ui/ws/frame_generator_delegate.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/server_window_compositor_frame_sink_manager.h"
#include "services/ui/ws/server_window_delegate.h"

namespace ui {

namespace ws {

FrameGenerator::FrameGenerator(FrameGeneratorDelegate* delegate,
                               ServerWindow* root_window)
    : delegate_(delegate),
      frame_sink_id_(
          WindowIdToTransportId(root_window->id()),
          static_cast<uint32_t>(mojom::CompositorFrameSinkType::DEFAULT)),
      root_window_(root_window),
      binding_(this),
      weak_factory_(this) {
  DCHECK(delegate_);
  surface_sequence_generator_.set_frame_sink_id(frame_sink_id_);
}

FrameGenerator::~FrameGenerator() {
  ReleaseAllSurfaceReferences();
  // Invalidate WeakPtrs now to avoid callbacks back into the
  // FrameGenerator during destruction of |compositor_frame_sink_|.
  weak_factory_.InvalidateWeakPtrs();
  compositor_frame_sink_.reset();
}

void FrameGenerator::OnGpuChannelEstablished(
    scoped_refptr<gpu::GpuChannelHost> channel) {
  if (widget_ != gfx::kNullAcceleratedWidget) {
    cc::mojom::MojoCompositorFrameSinkRequest request =
        mojo::GetProxy(&compositor_frame_sink_);
    // TODO(fsamuel): FrameGenerator should not know about
    // SurfacesContextProvider. In fact, FrameGenerator should not know
    // about GpuChannelHost.
    root_window_->CreateCompositorFrameSink(
        mojom::CompositorFrameSinkType::DEFAULT, widget_,
        channel->gpu_memory_buffer_manager(),
        new SurfacesContextProvider(widget_, channel), std::move(request),
        binding_.CreateInterfacePtrAndBind());
    // TODO(fsamuel): This means we're always requesting a new BeginFrame signal
    // even when we don't need it. Once surface ID propagation work is done,
    // this will not be necessary because FrameGenerator will only need a
    // BeginFrame if the window manager changes.
    compositor_frame_sink_->SetNeedsBeginFrame(true);
  } else {
    gpu_channel_ = std::move(channel);
  }
}

void FrameGenerator::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  if (gpu_channel_ && widget != gfx::kNullAcceleratedWidget) {
    cc::mojom::MojoCompositorFrameSinkRequest request =
        mojo::GetProxy(&compositor_frame_sink_);
    root_window_->CreateCompositorFrameSink(
        mojom::CompositorFrameSinkType::DEFAULT, widget_,
        gpu_channel_->gpu_memory_buffer_manager(),
        new SurfacesContextProvider(widget_, gpu_channel_),
        std::move(request), binding_.CreateInterfacePtrAndBind());
    // TODO(fsamuel): This means we're always requesting a new BeginFrame signal
    // even when we don't need it. Once surface ID propagation work is done,
    // this will not be necessary because FrameGenerator will only need a
    // BeginFrame if the window manager changes.
    compositor_frame_sink_->SetNeedsBeginFrame(true);
  }
}

void FrameGenerator::DidReceiveCompositorFrameAck() {}

void FrameGenerator::OnBeginFrame(const cc::BeginFrameArgs& begin_frame_arags) {
  if (!root_window_->visible())
    return;

  // TODO(fsamuel): We should add a trace for generating a top level frame.
  cc::CompositorFrame frame(GenerateCompositorFrame(root_window_->bounds()));
  if (compositor_frame_sink_)
    compositor_frame_sink_->SubmitCompositorFrame(std::move(frame));
}

void FrameGenerator::ReclaimResources(
    const cc::ReturnedResourceArray& resources) {
  // Nothing to do here because FrameGenerator CompositorFrames don't reference
  // any resources.
}

cc::CompositorFrame FrameGenerator::GenerateCompositorFrame(
    const gfx::Rect& output_rect) {
  const cc::RenderPassId render_pass_id(1, 1);
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(render_pass_id, output_rect, output_rect,
                      gfx::Transform());

  DrawWindowTree(render_pass.get(), root_window_, gfx::Vector2d(), 1.0f);

  std::unique_ptr<cc::DelegatedFrameData> frame_data(
      new cc::DelegatedFrameData);
  frame_data->render_pass_list.push_back(std::move(render_pass));
  if (delegate_->IsInHighContrastMode()) {
    std::unique_ptr<cc::RenderPass> invert_pass = cc::RenderPass::Create();
    invert_pass->SetNew(cc::RenderPassId(2, 0), output_rect, output_rect,
                        gfx::Transform());
    cc::SharedQuadState* shared_state =
        invert_pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(gfx::Transform(), output_rect.size(), output_rect,
                         output_rect, false, 1.f, SkXfermode::kSrcOver_Mode, 0);
    auto* quad = invert_pass->CreateAndAppendDrawQuad<cc::RenderPassDrawQuad>();
    cc::FilterOperations filters;
    filters.Append(cc::FilterOperation::CreateInvertFilter(1.f));
    quad->SetNew(shared_state, output_rect, output_rect, render_pass_id,
                 0 /* mask_resource_id */, gfx::Vector2dF() /* mask_uv_scale */,
                 gfx::Size() /* mask_texture_size */, filters,
                 gfx::Vector2dF() /* filters_scale */,
                 gfx::PointF() /* filters_origin */,
                 cc::FilterOperations() /* background_filters */);
    frame_data->render_pass_list.push_back(std::move(invert_pass));
  }

  cc::CompositorFrame frame;
  frame.delegated_frame_data = std::move(frame_data);
  return frame;
}

void FrameGenerator::DrawWindowTree(
    cc::RenderPass* pass,
    ServerWindow* window,
    const gfx::Vector2d& parent_to_root_origin_offset,
    float opacity) {
  if (!window->visible())
    return;

  const gfx::Rect absolute_bounds =
      window->bounds() + parent_to_root_origin_offset;
  const ServerWindow::Windows& children = window->children();
  const float combined_opacity = opacity * window->opacity();
  for (ServerWindow* child : base::Reversed(children)) {
    DrawWindowTree(pass, child, absolute_bounds.OffsetFromOrigin(),
                   combined_opacity);
  }

  if (!window->compositor_frame_sink_manager() ||
      !window->compositor_frame_sink_manager()->ShouldDraw())
    return;

  cc::SurfaceId underlay_surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(
          mojom::CompositorFrameSinkType::UNDERLAY);
  cc::SurfaceId default_surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(
          mojom::CompositorFrameSinkType::DEFAULT);

  if (!underlay_surface_id.is_valid() && !default_surface_id.is_valid())
    return;

  if (default_surface_id.is_valid()) {
    gfx::Transform quad_to_target_transform;
    quad_to_target_transform.Translate(absolute_bounds.x(),
                                       absolute_bounds.y());

    cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();

    const gfx::Rect bounds_at_origin(window->bounds().size());
    // TODO(fsamuel): These clipping and visible rects are incorrect. They need
    // to be populated from CompositorFrame structs.
    sqs->SetAll(quad_to_target_transform,
                bounds_at_origin.size() /* layer_bounds */,
                bounds_at_origin /* visible_layer_bounds */,
                bounds_at_origin /* clip_rect */, false /* is_clipped */,
                combined_opacity, SkXfermode::kSrcOver_Mode,
                0 /* sorting-context_id */);
    auto* quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    AddOrUpdateSurfaceReference(mojom::CompositorFrameSinkType::DEFAULT,
                                window);
    quad->SetAll(sqs, bounds_at_origin /* rect */,
                 gfx::Rect() /* opaque_rect */,
                 bounds_at_origin /* visible_rect */, true /* needs_blending*/,
                 default_surface_id);
  }
  if (underlay_surface_id.is_valid()) {
    const gfx::Rect underlay_absolute_bounds =
        absolute_bounds - window->underlay_offset();
    gfx::Transform quad_to_target_transform;
    quad_to_target_transform.Translate(underlay_absolute_bounds.x(),
                                       underlay_absolute_bounds.y());
    cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    const gfx::Rect bounds_at_origin(
        window->compositor_frame_sink_manager()->GetLatestFrameSize(
            mojom::CompositorFrameSinkType::UNDERLAY));
    sqs->SetAll(quad_to_target_transform,
                bounds_at_origin.size() /* layer_bounds */,
                bounds_at_origin /* visible_layer_bounds */,
                bounds_at_origin /* clip_rect */, false /* is_clipped */,
                combined_opacity, SkXfermode::kSrcOver_Mode,
                0 /* sorting-context_id */);

    auto* quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    AddOrUpdateSurfaceReference(mojom::CompositorFrameSinkType::UNDERLAY,
                                window);
    quad->SetAll(sqs, bounds_at_origin /* rect */,
                 gfx::Rect() /* opaque_rect */,
                 bounds_at_origin /* visible_rect */, true /* needs_blending*/,
                 underlay_surface_id);
  }
}

void FrameGenerator::AddOrUpdateSurfaceReference(
    mojom::CompositorFrameSinkType type,
    ServerWindow* window) {
  cc::SurfaceId surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(type);
  if (!surface_id.is_valid())
    return;
  auto it = dependencies_.find(surface_id.frame_sink_id());
  if (it == dependencies_.end()) {
    SurfaceDependency dependency = {
        surface_id.local_frame_id(),
        surface_sequence_generator_.CreateSurfaceSequence()};
    dependencies_[surface_id.frame_sink_id()] = dependency;
    GetDisplayCompositor()->AddSurfaceReference(surface_id,
                                                dependency.sequence);
    // Observe |window_surface|'s window so that we can release references when
    // the window is destroyed.
    Add(window);
    return;
  }

  // We are already holding a reference to this surface so there's no work to do
  // here.
  if (surface_id.local_frame_id() == it->second.local_frame_id)
    return;

  // If we have have an existing reference to a surface from the given
  // FrameSink, then we should release the reference, and then add this new
  // reference. This results in a delete and lookup in the map but simplifies
  // the code.
  ReleaseFrameSinkReference(surface_id.frame_sink_id());

  // This recursion will always terminate. This line is being called because
  // there was a stale surface reference. The stale reference has been released
  // in the previous line and cleared from the dependencies_ map. Thus, in the
  // recursive call, we'll enter the second if blcok because the FrameSinkId
  // is no longer referenced in the map.
  AddOrUpdateSurfaceReference(type, window);
}

void FrameGenerator::ReleaseFrameSinkReference(
    const cc::FrameSinkId& frame_sink_id) {
  auto it = dependencies_.find(frame_sink_id);
  if (it == dependencies_.end())
    return;
  std::vector<uint32_t> sequences;
  sequences.push_back(it->second.sequence.sequence);
  GetDisplayCompositor()->ReturnSurfaceReferences(frame_sink_id, sequences);
  dependencies_.erase(it);
}

void FrameGenerator::ReleaseAllSurfaceReferences() {
  std::vector<uint32_t> sequences;
  for (auto& dependency : dependencies_)
    sequences.push_back(dependency.second.sequence.sequence);
  GetDisplayCompositor()->ReturnSurfaceReferences(frame_sink_id_, sequences);
  dependencies_.clear();
}

ui::DisplayCompositor* FrameGenerator::GetDisplayCompositor() {
  return root_window_->delegate()->GetDisplayCompositor();
}

void FrameGenerator::OnWindowDestroying(ServerWindow* window) {
  Remove(window);
  ServerWindowCompositorFrameSinkManager* compositor_frame_sink_manager =
      window->compositor_frame_sink_manager();
  // If FrameGenerator was observing |window|, then that means it had a
  // CompositorFrame at some point in time and should have a
  // ServerWindowCompositorFrameSinkManager.
  DCHECK(compositor_frame_sink_manager);

  cc::SurfaceId default_surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(
          mojom::CompositorFrameSinkType::DEFAULT);
  if (default_surface_id.is_valid())
    ReleaseFrameSinkReference(default_surface_id.frame_sink_id());

  cc::SurfaceId underlay_surface_id =
      window->compositor_frame_sink_manager()->GetLatestSurfaceId(
          mojom::CompositorFrameSinkType::UNDERLAY);
  if (underlay_surface_id.is_valid())
    ReleaseFrameSinkReference(underlay_surface_id.frame_sink_id());
}

}  // namespace ws

}  // namespace ui
