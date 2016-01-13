// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer.h"

#include "cc/layers/surface_layer_impl.h"

namespace cc {

scoped_refptr<SurfaceLayer> SurfaceLayer::Create() {
  return make_scoped_refptr(new SurfaceLayer);
}

SurfaceLayer::SurfaceLayer() : Layer() {
}

SurfaceLayer::~SurfaceLayer() {}

void SurfaceLayer::SetSurfaceId(SurfaceId surface_id) {
  surface_id_ = surface_id;
  SetNeedsPushProperties();
}

scoped_ptr<LayerImpl> SurfaceLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return SurfaceLayerImpl::Create(tree_impl, id()).PassAs<LayerImpl>();
}

bool SurfaceLayer::DrawsContent() const {
  return !surface_id_.is_null() && Layer::DrawsContent();
}

void SurfaceLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);

  layer_impl->SetSurfaceId(surface_id_);
}

}  // namespace cc
