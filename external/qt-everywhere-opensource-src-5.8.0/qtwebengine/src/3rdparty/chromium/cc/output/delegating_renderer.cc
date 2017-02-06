// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/delegating_renderer.h"

#include <set>
#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/context_provider.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/resource_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"


namespace cc {

std::unique_ptr<DelegatingRenderer> DelegatingRenderer::Create(
    RendererClient* client,
    const RendererSettings* settings,
    OutputSurface* output_surface,
    ResourceProvider* resource_provider) {
  return base::WrapUnique(new DelegatingRenderer(
      client, settings, output_surface, resource_provider));
}

DelegatingRenderer::DelegatingRenderer(RendererClient* client,
                                       const RendererSettings* settings,
                                       OutputSurface* output_surface,
                                       ResourceProvider* resource_provider)
    : Renderer(client, settings),
      output_surface_(output_surface),
      resource_provider_(resource_provider) {
  DCHECK(resource_provider_);

  capabilities_.using_partial_swap = false;
  capabilities_.max_texture_size = resource_provider_->max_texture_size();
  capabilities_.best_texture_format = resource_provider_->best_texture_format();
  capabilities_.allow_partial_texture_updates =
      output_surface->capabilities().can_force_reclaim_resources;

  if (!output_surface_->context_provider()) {
    capabilities_.using_shared_memory_resources = true;
  } else {
    const auto& caps =
        output_surface_->context_provider()->ContextCapabilities();

    DCHECK(!caps.iosurface || caps.texture_rectangle);

    capabilities_.using_egl_image = caps.egl_image_external;
    capabilities_.using_image = caps.image;

    capabilities_.allow_rasterize_on_demand = false;

    // If MSAA is slow, we want this renderer to behave as though MSAA is not
    // available. Set samples to 0 to achieve this.
    if (caps.msaa_is_slow)
      capabilities_.max_msaa_samples = 0;
    else
      capabilities_.max_msaa_samples = caps.max_samples;
  }
}

DelegatingRenderer::~DelegatingRenderer() {}

const RendererCapabilitiesImpl& DelegatingRenderer::Capabilities() const {
  return capabilities_;
}

void DelegatingRenderer::DrawFrame(RenderPassList* render_passes_in_draw_order,
                                   float device_scale_factor,
                                   const gfx::ColorSpace& device_color_space,
                                   const gfx::Rect& device_viewport_rect,
                                   const gfx::Rect& device_clip_rect,
                                   bool disable_picture_quad_image_filtering) {
  TRACE_EVENT0("cc", "DelegatingRenderer::DrawFrame");

  DCHECK(!delegated_frame_data_);

  delegated_frame_data_ = base::WrapUnique(new DelegatedFrameData);
  DelegatedFrameData& out_data = *delegated_frame_data_;
  // Move the render passes and resources into the |out_frame|.
  out_data.render_pass_list.swap(*render_passes_in_draw_order);

  // Collect all resource ids in the render passes into a ResourceIdArray.
  ResourceProvider::ResourceIdArray resources;
  for (const auto& render_pass : out_data.render_pass_list) {
    for (const auto& quad : render_pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resources.push_back(resource_id);
    }
  }
  resource_provider_->PrepareSendToParent(resources, &out_data.resource_list);
}

void DelegatingRenderer::SwapBuffers(CompositorFrameMetadata metadata) {
  TRACE_EVENT0("cc,benchmark", "DelegatingRenderer::SwapBuffers");
  CompositorFrame compositor_frame;
  compositor_frame.metadata = std::move(metadata);
  compositor_frame.delegated_frame_data = std::move(delegated_frame_data_);
  output_surface_->SwapBuffers(std::move(compositor_frame));
}

void DelegatingRenderer::ReceiveSwapBuffersAck(
    const CompositorFrameAck& ack) {
  resource_provider_->ReceiveReturnsFromParent(ack.resources);
}

void DelegatingRenderer::DidChangeVisibility() {
  ContextProvider* context_provider = output_surface_->context_provider();
  if (!visible()) {
    TRACE_EVENT0("cc", "DelegatingRenderer::SetVisible dropping resources");
    if (context_provider) {
      context_provider->DeleteCachedResources();
      context_provider->ContextGL()->Flush();
    }
  }
  if (context_provider) {
    // If we are not visible, we ask the context to aggressively free resources.
    context_provider->ContextSupport()->SetAggressivelyFreeResources(
        !visible());
  }
}

}  // namespace cc
