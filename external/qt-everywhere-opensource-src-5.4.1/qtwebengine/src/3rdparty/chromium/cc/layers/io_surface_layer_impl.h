// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_IO_SURFACE_LAYER_IMPL_H_
#define CC_LAYERS_IO_SURFACE_LAYER_IMPL_H_

#include <string>

#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT IOSurfaceLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<IOSurfaceLayerImpl> Create(LayerTreeImpl* tree_impl,
                                               int id) {
    return make_scoped_ptr(new IOSurfaceLayerImpl(tree_impl, id));
  }
  virtual ~IOSurfaceLayerImpl();

  void SetIOSurfaceProperties(unsigned io_surface_id, const gfx::Size& size);

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer_tree_impl) OVERRIDE;

  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;

  virtual bool WillDraw(DrawMode draw_mode,
                        ResourceProvider* resource_provider) OVERRIDE;
  virtual void ReleaseResources() OVERRIDE;

 private:
  IOSurfaceLayerImpl(LayerTreeImpl* tree_impl, int id);

  void DestroyResource();

  virtual const char* LayerTypeAsString() const OVERRIDE;

  unsigned io_surface_id_;
  gfx::Size io_surface_size_;
  bool io_surface_changed_;
  unsigned io_surface_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(IOSurfaceLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_IO_SURFACE_LAYER_IMPL_H_
