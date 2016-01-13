// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/bitmap_content_layer_updater.h"

#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update.h"
#include "cc/resources/resource_update_queue.h"
#include "skia/ext/platform_canvas.h"

namespace cc {

BitmapContentLayerUpdater::Resource::Resource(
    BitmapContentLayerUpdater* updater,
    scoped_ptr<PrioritizedResource> texture)
    : LayerUpdater::Resource(texture.Pass()), updater_(updater) {}

BitmapContentLayerUpdater::Resource::~Resource() {}

void BitmapContentLayerUpdater::Resource::Update(
    ResourceUpdateQueue* queue,
    const gfx::Rect& source_rect,
    const gfx::Vector2d& dest_offset,
    bool partial_update) {
  updater_->UpdateTexture(
      queue, texture(), source_rect, dest_offset, partial_update);
}

scoped_refptr<BitmapContentLayerUpdater> BitmapContentLayerUpdater::Create(
    scoped_ptr<LayerPainter> painter,
    RenderingStatsInstrumentation* stats_instrumentation,
    int layer_id) {
  return make_scoped_refptr(
      new BitmapContentLayerUpdater(painter.Pass(),
                                    stats_instrumentation,
                                    layer_id));
}

BitmapContentLayerUpdater::BitmapContentLayerUpdater(
    scoped_ptr<LayerPainter> painter,
    RenderingStatsInstrumentation* stats_instrumentation,
    int layer_id)
    : ContentLayerUpdater(painter.Pass(), stats_instrumentation, layer_id) {}

BitmapContentLayerUpdater::~BitmapContentLayerUpdater() {}

scoped_ptr<LayerUpdater::Resource> BitmapContentLayerUpdater::CreateResource(
    PrioritizedResourceManager* manager) {
  return scoped_ptr<LayerUpdater::Resource>(
      new Resource(this, PrioritizedResource::Create(manager)));
}

void BitmapContentLayerUpdater::PrepareToUpdate(
    const gfx::Rect& content_rect,
    const gfx::Size& tile_size,
    float contents_width_scale,
    float contents_height_scale,
    gfx::Rect* resulting_opaque_rect) {
  if (canvas_size_ != content_rect.size()) {
    devtools_instrumentation::ScopedLayerTask paint_setup(
        devtools_instrumentation::kPaintSetup, layer_id_);
    canvas_size_ = content_rect.size();
    bool alloc = bitmap_backing_.allocN32Pixels(
        canvas_size_.width(), canvas_size_.height(), layer_is_opaque_);
    // TODO(danak): Remove when skia does the check for us: crbug.com/360384
    CHECK(alloc);
    canvas_ = skia::AdoptRef(new SkCanvas(bitmap_backing_));
    DCHECK_EQ(content_rect.width(), canvas_->getBaseLayerSize().width());
    DCHECK_EQ(content_rect.height(), canvas_->getBaseLayerSize().height());
  }

  base::TimeTicks start_time =
      rendering_stats_instrumentation_->StartRecording();
  PaintContents(canvas_.get(),
                content_rect,
                contents_width_scale,
                contents_height_scale,
                resulting_opaque_rect);
  base::TimeDelta duration =
      rendering_stats_instrumentation_->EndRecording(start_time);
  rendering_stats_instrumentation_->AddPaint(
      duration,
      content_rect.width() * content_rect.height());
}

void BitmapContentLayerUpdater::UpdateTexture(ResourceUpdateQueue* queue,
                                              PrioritizedResource* texture,
                                              const gfx::Rect& source_rect,
                                              const gfx::Vector2d& dest_offset,
                                              bool partial_update) {
  CHECK(canvas_);
  ResourceUpdate upload = ResourceUpdate::Create(texture,
                                                 &bitmap_backing_,
                                                 content_rect(),
                                                 source_rect,
                                                 dest_offset);
  if (partial_update)
    queue->AppendPartialUpload(upload);
  else
    queue->AppendFullUpload(upload);
}

void BitmapContentLayerUpdater::ReduceMemoryUsage() {
  canvas_.clear();
  canvas_size_ = gfx::Size();
}

void BitmapContentLayerUpdater::SetOpaque(bool opaque) {
  if (opaque != layer_is_opaque_) {
    canvas_.clear();
    canvas_size_ = gfx::Size();
  }

  ContentLayerUpdater::SetOpaque(opaque);
}

}  // namespace cc
