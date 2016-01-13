// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/contents_scaling_layer.h"
#include "ui/gfx/size_conversions.h"

namespace cc {

gfx::Size ContentsScalingLayer::ComputeContentBoundsForScale(
    float scale_x,
    float scale_y) const {
  return gfx::ToCeiledSize(gfx::ScaleSize(bounds(), scale_x, scale_y));
}

ContentsScalingLayer::ContentsScalingLayer()
    : last_update_contents_scale_x_(0.f),
      last_update_contents_scale_y_(0.f) {}

ContentsScalingLayer::~ContentsScalingLayer() {
}

void ContentsScalingLayer::CalculateContentsScale(
    float ideal_contents_scale,
    float device_scale_factor,
    float page_scale_factor,
    float maximum_animation_contents_scale,
    bool animating_transform_to_screen,
    float* contents_scale_x,
    float* contents_scale_y,
    gfx::Size* content_bounds) {
  *contents_scale_x = ideal_contents_scale;
  *contents_scale_y = ideal_contents_scale;
  *content_bounds = ComputeContentBoundsForScale(
      ideal_contents_scale,
      ideal_contents_scale);
}

bool ContentsScalingLayer::Update(ResourceUpdateQueue* queue,
                                  const OcclusionTracker<Layer>* occlusion) {
  bool updated = Layer::Update(queue, occlusion);

  if (draw_properties().contents_scale_x == last_update_contents_scale_x_ &&
      draw_properties().contents_scale_y == last_update_contents_scale_y_)
    return updated;

  last_update_contents_scale_x_ = draw_properties().contents_scale_x;
  last_update_contents_scale_y_ = draw_properties().contents_scale_y;
  // Invalidate the whole layer if scale changed.
  SetNeedsDisplayRect(gfx::Rect(paint_properties().bounds));
  return updated;
}

}  // namespace cc
