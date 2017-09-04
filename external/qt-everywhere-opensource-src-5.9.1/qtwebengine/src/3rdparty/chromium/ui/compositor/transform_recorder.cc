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
    context_.list_->CreateAndAppendPairedEndItem<cc::EndTransformDisplayItem>();
}

void TransformRecorder::Transform(const gfx::Transform& transform) {
  DCHECK(!transformed_);
  context_.list_->CreateAndAppendPairedBeginItem<cc::TransformDisplayItem>(
      transform);
  transformed_ = true;
}

}  // namespace ui
