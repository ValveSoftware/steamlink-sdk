// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SURFACE_LAYER_IMPL_H_
#define CC_LAYERS_SURFACE_LAYER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "cc/surfaces/surface_id.h"

namespace cc {

class CC_EXPORT SurfaceLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<SurfaceLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                  int id) {
    return base::WrapUnique(new SurfaceLayerImpl(tree_impl, id));
  }
  ~SurfaceLayerImpl() override;

  void SetSurfaceId(SurfaceId surface_id);
  void SetSurfaceScale(float scale);
  void SetSurfaceSize(const gfx::Size& size);
  SurfaceId surface_id() const { return surface_id_; }

  // LayerImpl overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;
  void AppendQuads(RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

 protected:
  SurfaceLayerImpl(LayerTreeImpl* tree_impl, int id);

 private:
  void GetDebugBorderProperties(SkColor* color, float* width) const override;
  void AppendRainbowDebugBorder(RenderPass* render_pass);
  void AsValueInto(base::trace_event::TracedValue* dict) const override;
  const char* LayerTypeAsString() const override;

  SurfaceId surface_id_;
  gfx::Size surface_size_;
  float surface_scale_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_SURFACE_LAYER_IMPL_H_
