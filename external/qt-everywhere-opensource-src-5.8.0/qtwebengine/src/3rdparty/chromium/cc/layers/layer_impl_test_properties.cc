// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_impl_test_properties.h"

#include "cc/layers/layer_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

LayerImplTestProperties::LayerImplTestProperties(LayerImpl* owning_layer)
    : owning_layer(owning_layer),
      double_sided(true),
      force_render_surface(false),
      is_container_for_fixed_position_layers(false),
      should_flatten_transform(true),
      hide_layer_and_subtree(false),
      opacity_can_animate(false),
      num_descendants_that_draw_content(0),
      num_unclipped_descendants(0),
      opacity(1.f),
      scroll_parent(nullptr),
      clip_parent(nullptr),
      mask_layer(nullptr),
      replica_layer(nullptr),
      parent(nullptr) {}

LayerImplTestProperties::~LayerImplTestProperties() {}

void LayerImplTestProperties::AddChild(std::unique_ptr<LayerImpl> child) {
  child->test_properties()->parent = owning_layer;
  children.push_back(child.get());
  owning_layer->layer_tree_impl()->AddLayer(std::move(child));
  owning_layer->layer_tree_impl()->BuildLayerListForTesting();
}

std::unique_ptr<LayerImpl> LayerImplTestProperties::RemoveChild(
    LayerImpl* child) {
  auto it = std::find(children.begin(), children.end(), child);
  if (it != children.end())
    children.erase(it);
  owning_layer->layer_tree_impl()->property_trees()->RemoveIdFromIdToIndexMaps(
      child->id());
  auto layer = owning_layer->layer_tree_impl()->RemoveLayer(child->id());
  owning_layer->layer_tree_impl()->BuildLayerListForTesting();
  return layer;
}

void LayerImplTestProperties::SetMaskLayer(std::unique_ptr<LayerImpl> mask) {
  if (mask_layer)
    owning_layer->layer_tree_impl()->RemoveLayer(mask_layer->id());
  mask_layer = mask.get();
  if (mask)
    owning_layer->layer_tree_impl()->AddLayer(std::move(mask));
}

void LayerImplTestProperties::SetReplicaLayer(
    std::unique_ptr<LayerImpl> replica) {
  if (replica_layer) {
    replica_layer->test_properties()->SetMaskLayer(nullptr);
    owning_layer->layer_tree_impl()->RemoveLayer(replica_layer->id());
  }
  replica_layer = replica.get();
  if (replica) {
    replica->test_properties()->parent = owning_layer;
    owning_layer->layer_tree_impl()->AddLayer(std::move(replica));
  }
}

}  // namespace cc
