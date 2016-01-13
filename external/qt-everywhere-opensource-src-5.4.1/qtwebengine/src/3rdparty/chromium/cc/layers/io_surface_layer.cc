// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/io_surface_layer.h"

#include "cc/layers/io_surface_layer_impl.h"

namespace cc {

scoped_refptr<IOSurfaceLayer> IOSurfaceLayer::Create() {
  return make_scoped_refptr(new IOSurfaceLayer());
}

IOSurfaceLayer::IOSurfaceLayer() : Layer(), io_surface_id_(0) {}

IOSurfaceLayer::~IOSurfaceLayer() {}

void IOSurfaceLayer::SetIOSurfaceProperties(uint32_t io_surface_id,
                                            const gfx::Size& size) {
  io_surface_id_ = io_surface_id;
  io_surface_size_ = size;
  SetNeedsCommit();
}

scoped_ptr<LayerImpl> IOSurfaceLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return IOSurfaceLayerImpl::Create(tree_impl, layer_id_).PassAs<LayerImpl>();
}

bool IOSurfaceLayer::DrawsContent() const {
  return io_surface_id_ && Layer::DrawsContent();
}

void IOSurfaceLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);

  IOSurfaceLayerImpl* io_surface_layer =
      static_cast<IOSurfaceLayerImpl*>(layer);
  io_surface_layer->SetIOSurfaceProperties(io_surface_id_, io_surface_size_);
}

bool IOSurfaceLayer::Update(ResourceUpdateQueue* queue,
                            const OcclusionTracker<Layer>* occlusion) {
  bool updated = Layer::Update(queue, occlusion);

  // This layer doesn't update any resources from the main thread side,
  // but repaint rects need to be sent to the layer impl via commit.
  return updated || !update_rect_.IsEmpty();
}

}  // namespace cc
