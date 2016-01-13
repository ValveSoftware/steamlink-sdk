// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_IMAGE_LAYER_IMPL_H_
#define CC_LAYERS_PICTURE_IMAGE_LAYER_IMPL_H_

#include "cc/layers/picture_layer_impl.h"

namespace cc {

class CC_EXPORT PictureImageLayerImpl : public PictureLayerImpl {
 public:
  static scoped_ptr<PictureImageLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                  int id) {
    return make_scoped_ptr(new PictureImageLayerImpl(tree_impl, id));
  }
  virtual ~PictureImageLayerImpl();

  // LayerImpl overrides.
  virtual const char* LayerTypeAsString() const OVERRIDE;
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) OVERRIDE;

 protected:
  PictureImageLayerImpl(LayerTreeImpl* tree_impl, int id);

  virtual bool ShouldAdjustRasterScale() const OVERRIDE;
  virtual void RecalculateRasterScales() OVERRIDE;
  virtual void GetDebugBorderProperties(
      SkColor* color, float* width) const OVERRIDE;

  virtual void UpdateIdealScales() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PictureImageLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_IMAGE_LAYER_IMPL_H_
