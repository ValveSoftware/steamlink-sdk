// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_BITMAP_CONTENT_LAYER_UPDATER_H_
#define CC_RESOURCES_BITMAP_CONTENT_LAYER_UPDATER_H_

#include "cc/base/cc_export.h"
#include "cc/resources/content_layer_updater.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"

class SkCanvas;

namespace cc {

class LayerPainter;
class RenderingStatsInstrumenation;

// This class rasterizes the content_rect into a skia bitmap canvas. It then
// creates a ResourceUpdate with this bitmap canvas and inserts the
// ResourceBundle to the provided ResourceUpdateQueue. Actual texture uploading
// is done by ResourceUpdateController.
class CC_EXPORT BitmapContentLayerUpdater : public ContentLayerUpdater {
 public:
  class Resource : public LayerUpdater::Resource {
   public:
    Resource(BitmapContentLayerUpdater* updater,
             scoped_ptr<PrioritizedResource> resource);
    virtual ~Resource();

    virtual void Update(ResourceUpdateQueue* queue,
                        const gfx::Rect& source_rect,
                        const gfx::Vector2d& dest_offset,
                        bool partial_update) OVERRIDE;

   private:
    BitmapContentLayerUpdater* updater_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };

  static scoped_refptr<BitmapContentLayerUpdater> Create(
      scoped_ptr<LayerPainter> painter,
      RenderingStatsInstrumentation* stats_instrumenation,
      int layer_id);

  virtual scoped_ptr<LayerUpdater::Resource> CreateResource(
      PrioritizedResourceManager* manager) OVERRIDE;
  virtual void PrepareToUpdate(const gfx::Rect& content_rect,
                               const gfx::Size& tile_size,
                               float contents_width_scale,
                               float contents_height_scale,
                               gfx::Rect* resulting_opaque_rect) OVERRIDE;
  void UpdateTexture(ResourceUpdateQueue* queue,
                     PrioritizedResource* resource,
                     const gfx::Rect& source_rect,
                     const gfx::Vector2d& dest_offset,
                     bool partial_update);
  virtual void SetOpaque(bool opaque) OVERRIDE;
  virtual void ReduceMemoryUsage() OVERRIDE;

 protected:
  BitmapContentLayerUpdater(
      scoped_ptr<LayerPainter> painter,
      RenderingStatsInstrumentation* stats_instrumenation,
      int layer_id);
  virtual ~BitmapContentLayerUpdater();

  SkBitmap bitmap_backing_;
  skia::RefPtr<SkCanvas> canvas_;
  gfx::Size canvas_size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BitmapContentLayerUpdater);
};

}  // namespace cc

#endif  // CC_RESOURCES_BITMAP_CONTENT_LAYER_UPDATER_H_
