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
    : context_(context), num_closers_(0) {
    }

ClipRecorder::~ClipRecorder() {
  for (int i = num_closers_ - 1; i >= 0; --i) {
    switch (closers_[i]) {
      case CLIP_RECT:
        context_.list_->CreateAndAppendPairedEndItem<cc::EndClipDisplayItem>();
        break;
      case CLIP_PATH:
        context_.list_
            ->CreateAndAppendPairedEndItem<cc::EndClipPathDisplayItem>();
        break;
    }
  }
}

void ClipRecorder::RecordCloser(Closer closer) {
  DCHECK_LT(num_closers_, kMaxOpCount);
  closers_[num_closers_++] = closer;
}

void ClipRecorder::ClipRect(const gfx::Rect& clip_rect) {
  bool antialias = false;
  context_.list_->CreateAndAppendPairedBeginItem<cc::ClipDisplayItem>(
      clip_rect, std::vector<SkRRect>(), antialias);
  RecordCloser(CLIP_RECT);
}

void ClipRecorder::ClipPath(const gfx::Path& clip_path) {
  bool antialias = false;
  context_.list_->CreateAndAppendPairedBeginItem<cc::ClipPathDisplayItem>(
      clip_path, SkRegion::kIntersect_Op, antialias);
  RecordCloser(CLIP_PATH);
}

void ClipRecorder::ClipPathWithAntiAliasing(const gfx::Path& clip_path) {
  bool antialias = true;
  context_.list_->CreateAndAppendPairedBeginItem<cc::ClipPathDisplayItem>(
      clip_path, SkRegion::kIntersect_Op, antialias);
  RecordCloser(CLIP_PATH);
}

}  // namespace ui
