// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_CONTENT_LAYER_H_
#define CC_LAYERS_CONTENT_LAYER_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "cc/layers/tiled_layer.h"
#include "cc/resources/layer_painter.h"

class SkCanvas;

namespace cc {

class ContentLayerClient;
class ContentLayerUpdater;

class CC_EXPORT ContentLayerPainter : public LayerPainter {
 public:
  static scoped_ptr<ContentLayerPainter> Create(ContentLayerClient* client);

  virtual void Paint(SkCanvas* canvas,
                     const gfx::Rect& content_rect,
                     gfx::RectF* opaque) OVERRIDE;

 private:
  explicit ContentLayerPainter(ContentLayerClient* client);

  ContentLayerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(ContentLayerPainter);
};

// A layer that renders its contents into an SkCanvas.
class CC_EXPORT ContentLayer : public TiledLayer {
 public:
  static scoped_refptr<ContentLayer> Create(ContentLayerClient* client);

  void ClearClient() { client_ = NULL; }

  virtual bool DrawsContent() const OVERRIDE;
  virtual void SetLayerTreeHost(LayerTreeHost* layer_tree_host) OVERRIDE;
  virtual void SetTexturePriorities(const PriorityCalculator& priority_calc)
      OVERRIDE;
  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker<Layer>* occlusion) OVERRIDE;
  virtual bool NeedMoreUpdates() OVERRIDE;

  virtual void SetContentsOpaque(bool contents_opaque) OVERRIDE;

  virtual bool SupportsLCDText() const OVERRIDE;

  virtual skia::RefPtr<SkPicture> GetPicture() const OVERRIDE;

  virtual void OnOutputSurfaceCreated() OVERRIDE;

 protected:
  explicit ContentLayer(ContentLayerClient* client);
  virtual ~ContentLayer();

  // TiledLayer implementation.
  virtual LayerUpdater* Updater() const OVERRIDE;

 private:
  // TiledLayer implementation.
  virtual void CreateUpdaterIfNeeded() OVERRIDE;

  void UpdateCanUseLCDText();

  ContentLayerClient* client_;
  scoped_refptr<ContentLayerUpdater> updater_;
  bool can_use_lcd_text_last_frame_;

  DISALLOW_COPY_AND_ASSIGN(ContentLayer);
};

}  // namespace cc
#endif  // CC_LAYERS_CONTENT_LAYER_H_
