// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_IO_SURFACE_LAYER_H_
#define CC_LAYERS_IO_SURFACE_LAYER_H_

#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"

namespace cc {

class CC_EXPORT IOSurfaceLayer : public Layer {
 public:
  static scoped_refptr<IOSurfaceLayer> Create();

  void SetIOSurfaceProperties(uint32_t io_surface_id, const gfx::Size& size);

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker<Layer>* occlusion) OVERRIDE;

 protected:
  IOSurfaceLayer();

 private:
  virtual ~IOSurfaceLayer();

  uint32_t io_surface_id_;
  gfx::Size io_surface_size_;

  DISALLOW_COPY_AND_ASSIGN(IOSurfaceLayer);
};

}  // namespace cc
#endif  // CC_LAYERS_IO_SURFACE_LAYER_H_
