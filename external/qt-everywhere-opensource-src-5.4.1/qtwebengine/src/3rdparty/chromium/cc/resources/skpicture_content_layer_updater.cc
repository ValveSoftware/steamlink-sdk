// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/skpicture_content_layer_updater.h"

#include "base/debug/trace_event.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update_queue.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace cc {

SkPictureContentLayerUpdater::SkPictureContentLayerUpdater(
    scoped_ptr<LayerPainter> painter,
    RenderingStatsInstrumentation* stats_instrumentation,
    int layer_id)
    : ContentLayerUpdater(painter.Pass(), stats_instrumentation, layer_id) {}

SkPictureContentLayerUpdater::~SkPictureContentLayerUpdater() {}

void SkPictureContentLayerUpdater::PrepareToUpdate(
    const gfx::Rect& content_rect,
    const gfx::Size&,
    float contents_width_scale,
    float contents_height_scale,
    gfx::Rect* resulting_opaque_rect) {
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(
      content_rect.width(), content_rect.height(), NULL, 0);
  DCHECK_EQ(content_rect.width(), canvas->getBaseLayerSize().width());
  DCHECK_EQ(content_rect.height(), canvas->getBaseLayerSize().height());
  base::TimeTicks start_time =
      rendering_stats_instrumentation_->StartRecording();
  PaintContents(canvas,
                content_rect,
                contents_width_scale,
                contents_height_scale,
                resulting_opaque_rect);
  base::TimeDelta duration =
      rendering_stats_instrumentation_->EndRecording(start_time);
  rendering_stats_instrumentation_->AddRecord(
      duration, content_rect.width() * content_rect.height());
  picture_ = skia::AdoptRef(recorder.endRecording());
}

void SkPictureContentLayerUpdater::DrawPicture(SkCanvas* canvas) {
  TRACE_EVENT0("cc", "SkPictureContentLayerUpdater::DrawPicture");
  if (picture_)
    canvas->drawPicture(picture_.get());
}

}  // namespace cc
