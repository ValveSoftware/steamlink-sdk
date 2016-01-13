// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_DELEGATE_H_
#define UI_COMPOSITOR_LAYER_DELEGATE_H_

#include "base/callback_forward.h"
#include "ui/compositor/compositor_export.h"

namespace gfx {
class Canvas;
}

namespace ui {

// A delegate interface implemented by an object that renders to a Layer.
class COMPOSITOR_EXPORT LayerDelegate {
 public:
  // Paint content for the layer to the specified canvas. It has already been
  // clipped to the Layer's invalid rect.
  virtual void OnPaintLayer(gfx::Canvas* canvas) = 0;

  // Called when the layer's device scale factor has changed.
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) = 0;

  // Invoked prior to the bounds changing. The returned closured is run after
  // the bounds change.
  virtual base::Closure PrepareForLayerBoundsChange() = 0;

 protected:
  virtual ~LayerDelegate() {}
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_DELEGATE_H_
