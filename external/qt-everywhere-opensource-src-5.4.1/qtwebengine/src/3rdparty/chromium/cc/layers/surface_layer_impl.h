// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SURFACE_LAYER_IMPL_H_
#define CC_LAYERS_SURFACE_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/surfaces/surface_id.h"

namespace cc {

class CC_EXPORT SurfaceLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<SurfaceLayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new SurfaceLayerImpl(tree_impl, id));
  }
  virtual ~SurfaceLayerImpl();

  void SetSurfaceId(SurfaceId surface_id);

  // LayerImpl overrides.
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;

 protected:
  SurfaceLayerImpl(LayerTreeImpl* tree_impl, int id);

 private:
  virtual void GetDebugBorderProperties(SkColor* color,
                                        float* width) const OVERRIDE;
  virtual void AsValueInto(base::DictionaryValue* dict) const OVERRIDE;
  virtual const char* LayerTypeAsString() const OVERRIDE;

  SurfaceId surface_id_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_SURFACE_LAYER_IMPL_H_
