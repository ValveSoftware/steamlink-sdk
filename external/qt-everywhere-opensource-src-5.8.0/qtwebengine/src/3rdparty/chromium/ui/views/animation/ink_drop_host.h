// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HOST_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HOST_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}  // namespace ui

namespace views {

class InkDropRipple;
class InkDropHighlight;

// Used by the InkDrop to add and remove the ink drop layers from a host's layer
// tree. Typically the ink drop layer is added to a View's layer but it can also
// be added to a View's ancestor layer.
//
// Note: Some Views do not own a Layer, but you can use
// View::SetLayer(new ui::Layer(ui::LAYER_NOT_DRAWN)) to force the View to have
// a lightweight Layer that can parent the ink drop layer.
class VIEWS_EXPORT InkDropHost {
 public:
  InkDropHost() {}
  virtual ~InkDropHost() {}

  // Adds the |ink_drop_layer| in to a visible layer tree.
  virtual void AddInkDropLayer(ui::Layer* ink_drop_layer) = 0;

  // Removes |ink_drop_layer| from the layer tree.
  virtual void RemoveInkDropLayer(ui::Layer* ink_drop_layer) = 0;

  // Creates and returns the effect used for press.
  virtual std::unique_ptr<InkDropRipple> CreateInkDropRipple() const = 0;

  // Creates and returns the effect used for hover and focus.
  virtual std::unique_ptr<InkDropHighlight> CreateInkDropHighlight() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropHost);
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HOST_H_
