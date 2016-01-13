// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SURFACE_LAYER_H_
#define CC_LAYERS_SURFACE_LAYER_H_

#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/surfaces/surface_id.h"

namespace cc {

// A layer that renders a surface referencing the output of another compositor
// instance or client.
class CC_EXPORT SurfaceLayer : public Layer {
 public:
  static scoped_refptr<SurfaceLayer> Create();

  void SetSurfaceId(SurfaceId surface_id);

  // Layer overrides.
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

 protected:
  SurfaceLayer();

 private:
  virtual ~SurfaceLayer();

  SurfaceId surface_id_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_SURFACE_LAYER_H_
