// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/transform_recorder.h"

#include "cc/playback/display_item_list.h"
#include "cc/playback/transform_display_item.h"
#include "ui/compositor/paint_context.h"

namespace ui {

TransformRecorder::TransformRecorder(const PaintContext& context)
    : context_(context), transformed_(false) {}

TransformRecorder::~TransformRecorder() {
  if (transformed_)
    context_.list_->CreateAndAppendItem<cc::EndTransformDisplayItem>(
        bounds_in_layer_);
}

void TransformRecorder::Transform(const gfx::Transform& transform,
                                  const gfx::Size& size_in_context) {
  DCHECK(!transformed_);
  bounds_in_layer_ = context_.ToLayerSpaceBounds(size_in_context);
  context_.list_->CreateAndAppendItem<cc::TransformDisplayItem>(
      bounds_in_layer_, transform);
  transformed_ = true;
}

}  // namespace ui
