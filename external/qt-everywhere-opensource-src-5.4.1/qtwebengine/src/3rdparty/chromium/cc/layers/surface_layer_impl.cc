// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer_impl.h"

#include "cc/debug/debug_colors.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/surface_draw_quad.h"

namespace cc {

SurfaceLayerImpl::SurfaceLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {
}

SurfaceLayerImpl::~SurfaceLayerImpl() {}

scoped_ptr<LayerImpl> SurfaceLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SurfaceLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

void SurfaceLayerImpl::SetSurfaceId(SurfaceId surface_id) {
  if (surface_id_ == surface_id)
    return;

  surface_id_ = surface_id;
  NoteLayerPropertyChanged();
}

void SurfaceLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);

  layer_impl->SetSurfaceId(surface_id_);
}

void SurfaceLayerImpl::AppendQuads(QuadSink* quad_sink,
                                   AppendQuadsData* append_quads_data) {
  SharedQuadState* shared_quad_state = quad_sink->CreateSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

  AppendDebugBorderQuad(
      quad_sink, content_bounds(), shared_quad_state, append_quads_data);

  if (surface_id_.is_null())
    return;

  scoped_ptr<SurfaceDrawQuad> quad = SurfaceDrawQuad::Create();
  gfx::Rect quad_rect(content_bounds());
  gfx::Rect visible_quad_rect = quad_sink->UnoccludedContentRect(
      quad_rect, draw_properties().target_space_transform);
  if (visible_quad_rect.IsEmpty())
    return;
  quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, surface_id_);
  quad_sink->Append(quad.PassAs<DrawQuad>());
}

void SurfaceLayerImpl::GetDebugBorderProperties(SkColor* color,
                                                float* width) const {
  *color = DebugColors::SurfaceLayerBorderColor();
  *width = DebugColors::SurfaceLayerBorderWidth(layer_tree_impl());
}

void SurfaceLayerImpl::AsValueInto(base::DictionaryValue* dict) const {
  LayerImpl::AsValueInto(dict);
  dict->SetInteger("surface_id", surface_id_.id);
}

const char* SurfaceLayerImpl::LayerTypeAsString() const {
  return "cc::SurfaceLayerImpl";
}

}  // namespace cc
