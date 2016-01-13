// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_scrollbar_layer_impl.h"

#include <algorithm>

#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/layers/layer.h"
#include "cc/layers/quad_sink.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_ptr<PaintedScrollbarLayerImpl> PaintedScrollbarLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation) {
  return make_scoped_ptr(
      new PaintedScrollbarLayerImpl(tree_impl, id, orientation));
}

PaintedScrollbarLayerImpl::PaintedScrollbarLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    ScrollbarOrientation orientation)
    : ScrollbarLayerImplBase(tree_impl, id, orientation, false, false),
      track_ui_resource_id_(0),
      thumb_ui_resource_id_(0),
      thumb_thickness_(0),
      thumb_length_(0),
      track_start_(0),
      track_length_(0),
      vertical_adjust_(0.f),
      scroll_layer_id_(Layer::INVALID_ID) {}

PaintedScrollbarLayerImpl::~PaintedScrollbarLayerImpl() {}

scoped_ptr<LayerImpl> PaintedScrollbarLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PaintedScrollbarLayerImpl::Create(tree_impl, id(), orientation())
      .PassAs<LayerImpl>();
}

void PaintedScrollbarLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  ScrollbarLayerImplBase::PushPropertiesTo(layer);

  PaintedScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetThumbThickness(thumb_thickness_);
  scrollbar_layer->SetThumbLength(thumb_length_);
  scrollbar_layer->SetTrackStart(track_start_);
  scrollbar_layer->SetTrackLength(track_length_);

  scrollbar_layer->set_track_ui_resource_id(track_ui_resource_id_);
  scrollbar_layer->set_thumb_ui_resource_id(thumb_ui_resource_id_);
}

bool PaintedScrollbarLayerImpl::WillDraw(DrawMode draw_mode,
                                  ResourceProvider* resource_provider) {
  DCHECK(draw_mode != DRAW_MODE_RESOURCELESS_SOFTWARE);
  return LayerImpl::WillDraw(draw_mode, resource_provider);
}

void PaintedScrollbarLayerImpl::AppendQuads(
    QuadSink* quad_sink,
    AppendQuadsData* append_quads_data) {
  bool premultipled_alpha = true;
  bool flipped = false;
  gfx::PointF uv_top_left(0.f, 0.f);
  gfx::PointF uv_bottom_right(1.f, 1.f);
  gfx::Rect bounds_rect(bounds());
  gfx::Rect content_bounds_rect(content_bounds());

  SharedQuadState* shared_quad_state = quad_sink->CreateSharedQuadState();
  PopulateSharedQuadState(shared_quad_state);

  AppendDebugBorderQuad(
      quad_sink, content_bounds(), shared_quad_state, append_quads_data);

  gfx::Rect thumb_quad_rect = ComputeThumbQuadRect();
  gfx::Rect visible_thumb_quad_rect =
      quad_sink->UnoccludedContentRect(thumb_quad_rect, draw_transform());

  ResourceProvider::ResourceId thumb_resource_id =
      layer_tree_impl()->ResourceIdForUIResource(thumb_ui_resource_id_);
  ResourceProvider::ResourceId track_resource_id =
      layer_tree_impl()->ResourceIdForUIResource(track_ui_resource_id_);

  if (thumb_resource_id && !visible_thumb_quad_rect.IsEmpty()) {
    gfx::Rect opaque_rect;
    const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 thumb_quad_rect,
                 opaque_rect,
                 visible_thumb_quad_rect,
                 thumb_resource_id,
                 premultipled_alpha,
                 uv_top_left,
                 uv_bottom_right,
                 SK_ColorTRANSPARENT,
                 opacity,
                 flipped);
    quad_sink->Append(quad.PassAs<DrawQuad>());
  }

  gfx::Rect track_quad_rect = content_bounds_rect;
  gfx::Rect visible_track_quad_rect =
      quad_sink->UnoccludedContentRect(track_quad_rect, draw_transform());
  if (track_resource_id && !visible_track_quad_rect.IsEmpty()) {
    gfx::Rect opaque_rect(contents_opaque() ? track_quad_rect : gfx::Rect());
    const float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
    scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
    quad->SetNew(shared_quad_state,
                 track_quad_rect,
                 opaque_rect,
                 visible_track_quad_rect,
                 track_resource_id,
                 premultipled_alpha,
                 uv_top_left,
                 uv_bottom_right,
                 SK_ColorTRANSPARENT,
                 opacity,
                 flipped);
    quad_sink->Append(quad.PassAs<DrawQuad>());
  }
}

void PaintedScrollbarLayerImpl::SetThumbThickness(int thumb_thickness) {
  if (thumb_thickness_ == thumb_thickness)
    return;
  thumb_thickness_ = thumb_thickness;
  NoteLayerPropertyChanged();
}

int PaintedScrollbarLayerImpl::ThumbThickness() const {
  return thumb_thickness_;
}

void PaintedScrollbarLayerImpl::SetThumbLength(int thumb_length) {
  if (thumb_length_ == thumb_length)
    return;
  thumb_length_ = thumb_length;
  NoteLayerPropertyChanged();
}

int PaintedScrollbarLayerImpl::ThumbLength() const {
  return thumb_length_;
}

void PaintedScrollbarLayerImpl::SetTrackStart(int track_start) {
  if (track_start_ == track_start)
    return;
  track_start_ = track_start;
  NoteLayerPropertyChanged();
}

int PaintedScrollbarLayerImpl::TrackStart() const {
  return track_start_;
}

void PaintedScrollbarLayerImpl::SetTrackLength(int track_length) {
  if (track_length_ == track_length)
    return;
  track_length_ = track_length;
  NoteLayerPropertyChanged();
}

float PaintedScrollbarLayerImpl::TrackLength() const {
  return track_length_ + (orientation() == VERTICAL ? vertical_adjust() : 0);
}

bool PaintedScrollbarLayerImpl::IsThumbResizable() const {
  return false;
}

const char* PaintedScrollbarLayerImpl::LayerTypeAsString() const {
  return "cc::PaintedScrollbarLayerImpl";
}

}  // namespace cc
