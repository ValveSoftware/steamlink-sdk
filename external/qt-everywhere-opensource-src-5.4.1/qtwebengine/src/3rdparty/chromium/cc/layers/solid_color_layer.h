// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_LAYERS_SOLID_COLOR_LAYER_H_
#define CC_LAYERS_SOLID_COLOR_LAYER_H_

#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"

namespace cc {

// A Layer that renders a solid color. The color is specified by using
// SetBackgroundColor() on the base class.
class CC_EXPORT SolidColorLayer : public Layer {
 public:
  static scoped_refptr<SolidColorLayer> Create();

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  virtual void SetBackgroundColor(SkColor color) OVERRIDE;

 protected:
  SolidColorLayer();

 private:
  virtual ~SolidColorLayer();

  DISALLOW_COPY_AND_ASSIGN(SolidColorLayer);
};

}  // namespace cc
#endif  // CC_LAYERS_SOLID_COLOR_LAYER_H_
