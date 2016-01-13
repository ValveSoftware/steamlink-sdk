// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display.h"

#include "base/message_loop/message_loop.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/software_renderer.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface.h"

namespace cc {

static ResourceProvider::ResourceId ResourceRemapHelper(
    bool* invalid_frame,
    const ResourceProvider::ResourceIdMap& child_to_parent_map,
    ResourceProvider::ResourceIdArray* resources_in_frame,
    ResourceProvider::ResourceId id) {
  ResourceProvider::ResourceIdMap::const_iterator it =
      child_to_parent_map.find(id);
  if (it == child_to_parent_map.end()) {
    *invalid_frame = true;
    return 0;
  }

  DCHECK_EQ(it->first, id);
  ResourceProvider::ResourceId remapped_id = it->second;
  resources_in_frame->push_back(id);
  return remapped_id;
}

Display::Display(DisplayClient* client,
                 SurfaceManager* manager,
                 SharedBitmapManager* bitmap_manager)
    : client_(client),
      manager_(manager),
      aggregator_(manager),
      bitmap_manager_(bitmap_manager) {
}

Display::~Display() {
}

void Display::Resize(const gfx::Size& size) {
  current_surface_.reset(new Surface(manager_, this, size));
}

void Display::InitializeOutputSurface() {
  if (output_surface_)
    return;
  scoped_ptr<OutputSurface> output_surface = client_->CreateOutputSurface();
  if (!output_surface->BindToClient(this))
    return;

  int highp_threshold_min = 0;
  bool use_rgba_4444_texture_format = false;
  size_t id_allocation_chunk_size = 1;
  bool use_distance_field_text = false;
  scoped_ptr<ResourceProvider> resource_provider =
      ResourceProvider::Create(output_surface.get(),
                               bitmap_manager_,
                               highp_threshold_min,
                               use_rgba_4444_texture_format,
                               id_allocation_chunk_size,
                               use_distance_field_text);
  if (!resource_provider)
    return;

  if (output_surface->context_provider()) {
    TextureMailboxDeleter* texture_mailbox_deleter = NULL;
    scoped_ptr<GLRenderer> renderer =
        GLRenderer::Create(this,
                           &settings_,
                           output_surface.get(),
                           resource_provider.get(),
                           texture_mailbox_deleter,
                           highp_threshold_min);
    if (!renderer)
      return;
    renderer_ = renderer.Pass();
  } else {
    scoped_ptr<SoftwareRenderer> renderer = SoftwareRenderer::Create(
        this, &settings_, output_surface.get(), resource_provider.get());
    if (!renderer)
      return;
    renderer_ = renderer.Pass();
  }

  output_surface_ = output_surface.Pass();
  resource_provider_ = resource_provider.Pass();
  child_id_ = resource_provider_->CreateChild(
      base::Bind(&Display::ReturnResources, base::Unretained(this)));
}

bool Display::Draw() {
  if (!current_surface_)
    return false;

  InitializeOutputSurface();
  if (!output_surface_)
    return false;

  // TODO(jamesr): Use the surface aggregator instead.
  scoped_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
  CompositorFrame* current_frame = current_surface_->GetEligibleFrame();
  frame_data->resource_list =
      current_frame->delegated_frame_data->resource_list;
  RenderPass::CopyAll(current_frame->delegated_frame_data->render_pass_list,
                      &frame_data->render_pass_list);

  if (frame_data->render_pass_list.empty())
    return false;

  const ResourceProvider::ResourceIdMap& resource_map =
      resource_provider_->GetChildToParentMap(child_id_);
  resource_provider_->ReceiveFromChild(child_id_, frame_data->resource_list);

  bool invalid_frame = false;
  ResourceProvider::ResourceIdArray resources_in_frame;
  DrawQuad::ResourceIteratorCallback remap_resources_to_parent_callback =
      base::Bind(&ResourceRemapHelper,
                 &invalid_frame,
                 resource_map,
                 &resources_in_frame);
  for (size_t i = 0; i < frame_data->render_pass_list.size(); ++i) {
    RenderPass* pass = frame_data->render_pass_list[i];
    for (size_t j = 0; j < pass->quad_list.size(); ++j) {
      DrawQuad* quad = pass->quad_list[j];
      quad->IterateResources(remap_resources_to_parent_callback);
    }
  }

  if (invalid_frame)
    return false;
  resource_provider_->DeclareUsedResourcesFromChild(child_id_,
                                                    resources_in_frame);

  float device_scale_factor = 1.0f;
  gfx::Rect device_viewport_rect = gfx::Rect(current_surface_->size());
  gfx::Rect device_clip_rect = device_viewport_rect;
  bool disable_picture_quad_image_filtering = false;

  renderer_->DrawFrame(&frame_data->render_pass_list,
                       device_scale_factor,
                       device_viewport_rect,
                       device_clip_rect,
                       disable_picture_quad_image_filtering);
  CompositorFrameMetadata metadata;
  renderer_->SwapBuffers(metadata);
  return true;
}

SurfaceId Display::CurrentSurfaceId() {
  return current_surface_ ? current_surface_->surface_id() : SurfaceId();
}

void Display::ReturnResources(const ReturnedResourceArray& resources) {
}

}  // namespace cc
