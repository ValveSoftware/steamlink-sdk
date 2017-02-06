// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_display_output_surface.h"

#include "base/bind.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

SurfaceDisplayOutputSurface::SurfaceDisplayOutputSurface(
    SurfaceManager* surface_manager,
    SurfaceIdAllocator* surface_id_allocator,
    Display* display,
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<ContextProvider> worker_context_provider)
    : OutputSurface(std::move(context_provider),
                    std::move(worker_context_provider),
                    nullptr),
      surface_manager_(surface_manager),
      surface_id_allocator_(surface_id_allocator),
      display_(display),
      factory_(surface_manager, this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  capabilities_.delegated_rendering = true;
  capabilities_.adjust_deadline_for_parent = true;
  capabilities_.can_force_reclaim_resources = true;

  // Display and SurfaceDisplayOutputSurface share a GL context, so sync
  // points aren't needed when passing resources between them.
  capabilities_.delegated_sync_points_required = false;
  factory_.set_needs_sync_points(false);
}

SurfaceDisplayOutputSurface::SurfaceDisplayOutputSurface(
    SurfaceManager* surface_manager,
    SurfaceIdAllocator* surface_id_allocator,
    Display* display,
    scoped_refptr<VulkanContextProvider> vulkan_context_provider)
    : OutputSurface(std::move(vulkan_context_provider)),
      surface_manager_(surface_manager),
      surface_id_allocator_(surface_id_allocator),
      display_(display),
      factory_(surface_manager, this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  capabilities_.delegated_rendering = true;
  capabilities_.adjust_deadline_for_parent = true;
  capabilities_.can_force_reclaim_resources = true;
}

SurfaceDisplayOutputSurface::~SurfaceDisplayOutputSurface() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (HasClient())
    DetachFromClient();
}

void SurfaceDisplayOutputSurface::SwapBuffers(CompositorFrame frame) {
  gfx::Size frame_size =
      frame.delegated_frame_data->render_pass_list.back()->output_rect.size();
  if (frame_size.IsEmpty() || frame_size != last_swap_frame_size_) {
    if (!delegated_surface_id_.is_null()) {
      factory_.Destroy(delegated_surface_id_);
    }
    delegated_surface_id_ = surface_id_allocator_->GenerateId();
    factory_.Create(delegated_surface_id_);
    last_swap_frame_size_ = frame_size;
  }
  display_->SetSurfaceId(delegated_surface_id_,
                         frame.metadata.device_scale_factor);

  client_->DidSwapBuffers();

  factory_.SubmitCompositorFrame(
      delegated_surface_id_, std::move(frame),
      base::Bind(&SurfaceDisplayOutputSurface::SwapBuffersComplete,
                 base::Unretained(this)));
}

bool SurfaceDisplayOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  surface_manager_->RegisterSurfaceFactoryClient(
      surface_id_allocator_->id_namespace(), this);

  if (!OutputSurface::BindToClient(client))
    return false;

  // We want the Display's output surface to hear about lost context, and since
  // this shares a context with it, we should not be listening for lost context
  // callbacks on the context here.
  if (context_provider())
    context_provider()->SetLostContextCallback(base::Closure());

  // Avoid initializing GL context here, as this should be sharing the
  // Display's context.
  display_->Initialize(this);
  return true;
}

void SurfaceDisplayOutputSurface::ForceReclaimResources() {
  if (!delegated_surface_id_.is_null()) {
    factory_.SubmitCompositorFrame(delegated_surface_id_, CompositorFrame(),
                                   SurfaceFactory::DrawCallback());
  }
}

void SurfaceDisplayOutputSurface::DetachFromClient() {
  DCHECK(HasClient());
  // Unregister the SurfaceFactoryClient here instead of the dtor so that only
  // one client is alive for this namespace at any given time.
  surface_manager_->UnregisterSurfaceFactoryClient(
      surface_id_allocator_->id_namespace());
  if (!delegated_surface_id_.is_null())
    factory_.Destroy(delegated_surface_id_);

  OutputSurface::DetachFromClient();
}

void SurfaceDisplayOutputSurface::BindFramebuffer() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
}

uint32_t SurfaceDisplayOutputSurface::GetFramebufferCopyTextureFormat() {
  // This is a delegating output surface, no framebuffer/direct drawing support.
  NOTREACHED();
  return 0;
}

void SurfaceDisplayOutputSurface::ReturnResources(
    const ReturnedResourceArray& resources) {
  CompositorFrameAck ack;
  ack.resources = resources;
  if (client_)
    client_->ReclaimResources(&ack);
}

void SurfaceDisplayOutputSurface::SetBeginFrameSource(
    BeginFrameSource* begin_frame_source) {
  DCHECK(client_);
  client_->SetBeginFrameSource(begin_frame_source);
}

void SurfaceDisplayOutputSurface::DisplayOutputSurfaceLost() {
  output_surface_lost_ = true;
  DidLoseOutputSurface();
}

void SurfaceDisplayOutputSurface::DisplaySetMemoryPolicy(
    const ManagedMemoryPolicy& policy) {
  SetMemoryPolicy(policy);
}

void SurfaceDisplayOutputSurface::SwapBuffersComplete(SurfaceDrawStatus drawn) {
  // TODO(danakj): Why the lost check?
  if (!output_surface_lost_)
    client_->DidSwapBuffersComplete();
}

}  // namespace cc
