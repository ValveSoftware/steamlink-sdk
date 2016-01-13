// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/bitmap_skpicture_content_layer_updater.h"

#include "base/time/time.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update_queue.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

BitmapSkPictureContentLayerUpdater::Resource::Resource(
    BitmapSkPictureContentLayerUpdater* updater,
    scoped_ptr<PrioritizedResource> texture)
    : ContentLayerUpdater::Resource(texture.Pass()), updater_(updater) {}

void BitmapSkPictureContentLayerUpdater::Resource::Update(
    ResourceUpdateQueue* queue,
    const gfx::Rect& source_rect,
    const gfx::Vector2d& dest_offset,
    bool partial_update) {
  SkAlphaType at =
      updater_->layer_is_opaque() ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
  bitmap_.allocPixels(SkImageInfo::Make(
      source_rect.width(), source_rect.height(), kPMColor_SkColorType, at));
  SkCanvas canvas(bitmap_);
  updater_->PaintContentsRect(&canvas, source_rect);

  ResourceUpdate upload = ResourceUpdate::Create(
      texture(), &bitmap_, source_rect, source_rect, dest_offset);
  if (partial_update)
    queue->AppendPartialUpload(upload);
  else
    queue->AppendFullUpload(upload);
}

scoped_refptr<BitmapSkPictureContentLayerUpdater>
BitmapSkPictureContentLayerUpdater::Create(
    scoped_ptr<LayerPainter> painter,
    RenderingStatsInstrumentation* stats_instrumentation,
    int layer_id) {
  return make_scoped_refptr(
      new BitmapSkPictureContentLayerUpdater(painter.Pass(),
                                             stats_instrumentation,
                                             layer_id));
}

BitmapSkPictureContentLayerUpdater::BitmapSkPictureContentLayerUpdater(
    scoped_ptr<LayerPainter> painter,
    RenderingStatsInstrumentation* stats_instrumentation,
    int layer_id)
    : SkPictureContentLayerUpdater(painter.Pass(),
                                   stats_instrumentation,
                                   layer_id) {}

BitmapSkPictureContentLayerUpdater::~BitmapSkPictureContentLayerUpdater() {}

scoped_ptr<LayerUpdater::Resource>
BitmapSkPictureContentLayerUpdater::CreateResource(
    PrioritizedResourceManager* manager) {
  return scoped_ptr<LayerUpdater::Resource>(
      new Resource(this, PrioritizedResource::Create(manager)));
}

void BitmapSkPictureContentLayerUpdater::PaintContentsRect(
    SkCanvas* canvas,
    const gfx::Rect& source_rect) {
  if (!canvas)
    return;
  // Translate the origin of content_rect to that of source_rect.
  canvas->translate(content_rect().x() - source_rect.x(),
                    content_rect().y() - source_rect.y());
  base::TimeTicks start_time =
      rendering_stats_instrumentation_->StartRecording();
  DrawPicture(canvas);
  base::TimeDelta duration =
      rendering_stats_instrumentation_->EndRecording(start_time);
  rendering_stats_instrumentation_->AddRaster(
      duration,
      source_rect.width() * source_rect.height());
}

}  // namespace cc
