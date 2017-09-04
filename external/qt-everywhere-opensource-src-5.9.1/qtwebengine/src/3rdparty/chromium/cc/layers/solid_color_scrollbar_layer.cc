// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_scrollbar_layer.h"

#include <memory>

#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/proto/cc_conversions.h"
#include "cc/proto/layer.pb.h"

namespace cc {

std::unique_ptr<LayerImpl> SolidColorScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  const bool kIsOverlayScrollbar = true;
  return SolidColorScrollbarLayerImpl::Create(
      tree_impl, id(), orientation(),
      solid_color_scrollbar_layer_inputs_.thumb_thickness,
      solid_color_scrollbar_layer_inputs_.track_start,
      solid_color_scrollbar_layer_inputs_.is_left_side_vertical_scrollbar,
      kIsOverlayScrollbar);
}

scoped_refptr<SolidColorScrollbarLayer> SolidColorScrollbarLayer::Create(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar,
    int scroll_layer_id) {
  return make_scoped_refptr(new SolidColorScrollbarLayer(
      orientation, thumb_thickness, track_start,
      is_left_side_vertical_scrollbar, scroll_layer_id));
}

SolidColorScrollbarLayer::SolidColorScrollbarLayerInputs::
    SolidColorScrollbarLayerInputs(ScrollbarOrientation orientation,
                                   int thumb_thickness,
                                   int track_start,
                                   bool is_left_side_vertical_scrollbar,
                                   int scroll_layer_id)
    : scroll_layer_id(Layer::INVALID_ID),
      orientation(orientation),
      thumb_thickness(thumb_thickness),
      track_start(track_start),
      is_left_side_vertical_scrollbar(is_left_side_vertical_scrollbar) {}

SolidColorScrollbarLayer::SolidColorScrollbarLayerInputs::
    ~SolidColorScrollbarLayerInputs() = default;

SolidColorScrollbarLayer::SolidColorScrollbarLayer(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar,
    int scroll_layer_id)
    : solid_color_scrollbar_layer_inputs_(orientation,
                                          thumb_thickness,
                                          track_start,
                                          is_left_side_vertical_scrollbar,
                                          scroll_layer_id) {
  Layer::SetOpacity(0.f);
}

SolidColorScrollbarLayer::~SolidColorScrollbarLayer() {}

ScrollbarLayerInterface* SolidColorScrollbarLayer::ToScrollbarLayer() {
  return this;
}

void SolidColorScrollbarLayer::ToLayerNodeProto(proto::LayerNode* proto) const {
  Layer::ToLayerNodeProto(proto);

  proto::SolidColorScrollbarLayerProperties* scrollbar =
      proto->mutable_solid_scrollbar();
  scrollbar->set_scroll_layer_id(
      solid_color_scrollbar_layer_inputs_.scroll_layer_id);
  scrollbar->set_thumb_thickness(
      solid_color_scrollbar_layer_inputs_.thumb_thickness);
  scrollbar->set_track_start(solid_color_scrollbar_layer_inputs_.track_start);
  scrollbar->set_is_left_side_vertical_scrollbar(
      solid_color_scrollbar_layer_inputs_.is_left_side_vertical_scrollbar);
  scrollbar->set_orientation(ScrollbarOrientationToProto(
      solid_color_scrollbar_layer_inputs_.orientation));
}

void SolidColorScrollbarLayer::SetOpacity(float opacity) {
  // The opacity of a solid color scrollbar layer is always 0 on main thread.
  DCHECK_EQ(opacity, 0.f);
  Layer::SetOpacity(opacity);
}

void SolidColorScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  SolidColorScrollbarLayerImpl* scrollbar_layer =
      static_cast<SolidColorScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetScrollLayerId(
      solid_color_scrollbar_layer_inputs_.scroll_layer_id);
}

void SolidColorScrollbarLayer::SetNeedsDisplayRect(const gfx::Rect& rect) {
  // Never needs repaint.
}

bool SolidColorScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return true;
}

bool SolidColorScrollbarLayer::AlwaysUseActiveTreeOpacity() const {
  return true;
}

int SolidColorScrollbarLayer::ScrollLayerId() const {
  return solid_color_scrollbar_layer_inputs_.scroll_layer_id;
}

void SolidColorScrollbarLayer::SetScrollLayer(int layer_id) {
  if (layer_id == solid_color_scrollbar_layer_inputs_.scroll_layer_id)
    return;

  solid_color_scrollbar_layer_inputs_.scroll_layer_id = layer_id;
  SetNeedsFullTreeSync();
}

ScrollbarOrientation SolidColorScrollbarLayer::orientation() const {
  return solid_color_scrollbar_layer_inputs_.orientation;
}

void SolidColorScrollbarLayer::SetTypeForProtoSerialization(
    proto::LayerNode* proto) const {
  proto->set_type(proto::LayerNode::SOLID_COLOR_SCROLLBAR_LAYER);
}

}  // namespace cc
