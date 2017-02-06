// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/clip_recorder.h"

#include "cc/playback/clip_display_item.h"
#include "cc/playback/clip_path_display_item.h"
#include "cc/playback/display_item_list.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"

namespace ui {

ClipRecorder::ClipRecorder(const PaintContext& context)
    : context_(context), num_closers_(0) {}

ClipRecorder::~ClipRecorder() {
  for (int i = num_closers_ - 1; i >= 0; --i) {
    const gfx::Rect& bounds_in_layer = bounds_in_layer_[i];
    switch (closers_[i]) {
      case CLIP_RECT:
        context_.list_->CreateAndAppendItem<cc::EndClipDisplayItem>(
            bounds_in_layer);
        break;
      case CLIP_PATH:
        context_.list_->CreateAndAppendItem<cc::EndClipPathDisplayItem>(
            bounds_in_layer);
        break;
    }
  }
}

void ClipRecorder::RecordCloser(const gfx::Rect& bounds_in_layer,
                                Closer closer) {
  DCHECK_LT(num_closers_, kMaxOpCount);
  closers_[num_closers_] = closer;
  bounds_in_layer_[num_closers_++] = bounds_in_layer;
}

static gfx::Rect PathToEnclosingRect(const gfx::Path& path) {
  return gfx::ToEnclosingRect(gfx::SkRectToRectF(path.getBounds()));
}

void ClipRecorder::ClipRect(const gfx::Rect& clip_rect) {
  bool antialias = false;
  gfx::Rect clip_in_layer_space = context_.ToLayerSpaceRect(clip_rect);
  context_.list_->CreateAndAppendItem<cc::ClipDisplayItem>(
      clip_in_layer_space, clip_rect, std::vector<SkRRect>(), antialias);
  RecordCloser(clip_in_layer_space, CLIP_RECT);
}

void ClipRecorder::ClipPath(const gfx::Path& clip_path) {
  bool antialias = false;
  gfx::Rect clip_in_layer_space =
      context_.ToLayerSpaceRect(PathToEnclosingRect(clip_path));
  context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>(
      clip_in_layer_space, clip_path, SkRegion::kIntersect_Op, antialias);
  RecordCloser(clip_in_layer_space, CLIP_PATH);
}

void ClipRecorder::ClipPathWithAntiAliasing(const gfx::Path& clip_path) {
  bool antialias = true;
  gfx::Rect clip_in_layer_space =
      context_.ToLayerSpaceRect(PathToEnclosingRect(clip_path));
  context_.list_->CreateAndAppendItem<cc::ClipPathDisplayItem>(
      clip_in_layer_space, clip_path, SkRegion::kIntersect_Op, antialias);
  RecordCloser(clip_in_layer_space, CLIP_PATH);
}

}  // namespace ui
