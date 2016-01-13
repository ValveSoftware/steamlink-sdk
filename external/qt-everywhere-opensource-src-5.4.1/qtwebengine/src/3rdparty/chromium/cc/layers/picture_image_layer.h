// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_IMAGE_LAYER_H_
#define CC_LAYERS_PICTURE_IMAGE_LAYER_H_

#include "cc/base/cc_export.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT PictureImageLayer : public PictureLayer, ContentLayerClient {
 public:
  static scoped_refptr<PictureImageLayer> Create();

  void SetBitmap(const SkBitmap& image);

  // Layer implementation.
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE;

  // ContentLayerClient implementation.
  virtual void PaintContents(
      SkCanvas* canvas,
      const gfx::Rect& clip,
      gfx::RectF* opaque,
      ContentLayerClient::GraphicsContextStatus gc_status) OVERRIDE;
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}
  virtual bool FillsBoundsCompletely() const OVERRIDE;

 private:
  PictureImageLayer();
  virtual ~PictureImageLayer();

  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(PictureImageLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_IMAGE_LAYER_H_
