// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_
#define CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/layers/contents_scaling_layer.h"

namespace cc {

class CC_EXPORT HeadsUpDisplayLayer : public ContentsScalingLayer {
 public:
  static scoped_refptr<HeadsUpDisplayLayer> Create();

  void PrepareForCalculateDrawProperties(
      const gfx::Size& device_viewport, float device_scale_factor);

  virtual bool DrawsContent() const OVERRIDE;

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

 protected:
  HeadsUpDisplayLayer();

 private:
  virtual ~HeadsUpDisplayLayer();

  DISALLOW_COPY_AND_ASSIGN(HeadsUpDisplayLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_
