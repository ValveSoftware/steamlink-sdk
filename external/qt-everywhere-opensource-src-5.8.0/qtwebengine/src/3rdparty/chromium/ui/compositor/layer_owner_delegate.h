// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_OWNER_DELEGATE_H_
#define UI_COMPOSITOR_LAYER_OWNER_DELEGATE_H_

#include "ui/compositor/compositor_export.h"

namespace ui {
class Layer;

// Called from RecreateLayer() after the new layer was created. old_layer is
// the layer that will be returned to the caller of RecreateLayer, new_layer
// will be the layer now used. Used when the layer has external content
// (SetTextureMailbox / SetDelegatedFrame was called).
class COMPOSITOR_EXPORT LayerOwnerDelegate {
 public:
  virtual void OnLayerRecreated(Layer* old_layer, Layer* new_layer) = 0;

 protected:
  virtual ~LayerOwnerDelegate() {}
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_OWNER_DELEGATE_H_
