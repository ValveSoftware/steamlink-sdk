// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_
#define CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_

#include <set>
#include <vector>

#include "base/memory/ptr_util.h"
#include "cc/layers/layer_collections.h"
#include "cc/layers/layer_position_constraint.h"
#include "cc/output/filter_operations.h"
#include "ui/gfx/geometry/point3_f.h"

namespace cc {

class CopyOutputRequest;
class LayerImpl;

struct CC_EXPORT LayerImplTestProperties {
  explicit LayerImplTestProperties(LayerImpl* owning_layer);
  ~LayerImplTestProperties();

  void AddChild(std::unique_ptr<LayerImpl> child);
  std::unique_ptr<LayerImpl> RemoveChild(LayerImpl* child);
  void SetMaskLayer(std::unique_ptr<LayerImpl> mask);
  void SetReplicaLayer(std::unique_ptr<LayerImpl> replica);

  LayerImpl* owning_layer;
  bool double_sided;
  bool force_render_surface;
  bool is_container_for_fixed_position_layers;
  bool should_flatten_transform;
  bool hide_layer_and_subtree;
  bool opacity_can_animate;
  int num_descendants_that_draw_content;
  size_t num_unclipped_descendants;
  float opacity;
  FilterOperations background_filters;
  LayerPositionConstraint position_constraint;
  gfx::Point3F transform_origin;
  LayerImpl* scroll_parent;
  std::unique_ptr<std::set<LayerImpl*>> scroll_children;
  LayerImpl* clip_parent;
  std::unique_ptr<std::set<LayerImpl*>> clip_children;
  std::vector<std::unique_ptr<CopyOutputRequest>> copy_requests;
  LayerImplList children;
  LayerImpl* mask_layer;
  LayerImpl* replica_layer;
  LayerImpl* parent;
};

}  // namespace cc

#endif  // CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_
