// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_DELEGATE_H_
#define UI_COMPOSITOR_LAYER_DELEGATE_H_

#include "ui/compositor/compositor_export.h"

namespace gfx {
class Rect;
}

namespace ui {
class PaintContext;

// A delegate interface implemented by an object that renders to a Layer.
class COMPOSITOR_EXPORT LayerDelegate {
 public:
  // Paint content for the layer to the specified context.
  virtual void OnPaintLayer(const PaintContext& context) = 0;

  // A notification that this layer has had a delegated frame swap and
  // will be repainted.
  virtual void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) = 0;

  // Called when the layer's device scale factor has changed.
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) = 0;

  // Invoked when the bounds have changed.
  virtual void OnLayerBoundsChanged(const gfx::Rect& old_bounds);

 protected:
  virtual ~LayerDelegate() {}
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_DELEGATE_H_
