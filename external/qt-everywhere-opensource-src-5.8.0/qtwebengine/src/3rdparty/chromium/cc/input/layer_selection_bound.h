// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_LAYER_SELECTION_BOUND_H_
#define CC_INPUT_LAYER_SELECTION_BOUND_H_

#include "cc/base/cc_export.h"
#include "cc/input/selection.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/selection_bound.h"

namespace cc {

namespace proto {
class LayerSelection;
class LayerSelectionBound;
}  // namespace proto

// Marker for a selection end-point attached to a specific layer.
// TODO(fsamuel): This could be unified with gfx::SelectionBound.
struct CC_EXPORT LayerSelectionBound {
  LayerSelectionBound();
  ~LayerSelectionBound();

  gfx::SelectionBound::Type type;
  gfx::Point edge_top;
  gfx::Point edge_bottom;
  int layer_id;

  bool operator==(const LayerSelectionBound& other) const;
  bool operator!=(const LayerSelectionBound& other) const;

  void ToProtobuf(proto::LayerSelectionBound* proto) const;
  void FromProtobuf(const proto::LayerSelectionBound& proto);
};

typedef Selection<LayerSelectionBound> LayerSelection;

CC_EXPORT void LayerSelectionToProtobuf(const LayerSelection& selection,
                                        proto::LayerSelection* proto);
CC_EXPORT void LayerSelectionFromProtobuf(LayerSelection* selection,
                                          const proto::LayerSelection& proto);

}  // namespace cc

#endif  // CC_INPUT_LAYER_SELECTION_BOUND_H_
