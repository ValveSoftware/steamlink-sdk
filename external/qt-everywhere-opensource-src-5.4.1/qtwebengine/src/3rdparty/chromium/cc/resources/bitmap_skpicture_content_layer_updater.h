// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_BITMAP_SKPICTURE_CONTENT_LAYER_UPDATER_H_
#define CC_RESOURCES_BITMAP_SKPICTURE_CONTENT_LAYER_UPDATER_H_

#include "cc/resources/skpicture_content_layer_updater.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

// This class records the content_rect into an SkPicture, then software
// rasterizes the SkPicture into bitmaps for each tile. This implements
// LayerTreeSettingSettings::per_tile_painting_enabled.
class BitmapSkPictureContentLayerUpdater : public SkPictureContentLayerUpdater {
 public:
  class Resource : public ContentLayerUpdater::Resource {
   public:
    Resource(BitmapSkPictureContentLayerUpdater* updater,
             scoped_ptr<PrioritizedResource> texture);

    virtual void Update(ResourceUpdateQueue* queue,
                        const gfx::Rect& source_rect,
                        const gfx::Vector2d& dest_offset,
                        bool partial_update) OVERRIDE;

   private:
    SkBitmap bitmap_;
    BitmapSkPictureContentLayerUpdater* updater_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };

  static scoped_refptr<BitmapSkPictureContentLayerUpdater> Create(
      scoped_ptr<LayerPainter> painter,
      RenderingStatsInstrumentation* stats_instrumentation,
      int layer_id);

  virtual scoped_ptr<LayerUpdater::Resource> CreateResource(
      PrioritizedResourceManager* manager) OVERRIDE;
  void PaintContentsRect(SkCanvas* canvas,
                         const gfx::Rect& source_rect);

 private:
  BitmapSkPictureContentLayerUpdater(
      scoped_ptr<LayerPainter> painter,
      RenderingStatsInstrumentation* stats_instrumentation,
      int layer_id);
  virtual ~BitmapSkPictureContentLayerUpdater();

  DISALLOW_COPY_AND_ASSIGN(BitmapSkPictureContentLayerUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_BITMAP_SKPICTURE_CONTENT_LAYER_UPDATER_H_
