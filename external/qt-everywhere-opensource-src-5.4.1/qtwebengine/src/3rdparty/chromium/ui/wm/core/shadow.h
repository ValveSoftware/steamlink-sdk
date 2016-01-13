// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_SHADOW_H_
#define UI_WM_CORE_SHADOW_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/rect.h"
#include "ui/wm/wm_export.h"

namespace ui {
class Layer;
}  // namespace ui

namespace wm {

class ImageGrid;

// Simple class that draws a drop shadow around content at given bounds.
class WM_EXPORT Shadow : public ui::ImplicitAnimationObserver {
 public:
  enum Style {
    // Active windows have more opaque shadows, shifted down to make the window
    // appear "higher".
    STYLE_ACTIVE,

    // Inactive windows have less opaque shadows.
    STYLE_INACTIVE,

    // Small windows like tooltips and context menus have lighter, smaller
    // shadows.
    STYLE_SMALL,
  };

  Shadow();
  virtual ~Shadow();

  void Init(Style style);

  // Returns |image_grid_|'s ui::Layer.  This is exposed so it can be added to
  // the same layer as the content and stacked below it.  SetContentBounds()
  // should be used to adjust the shadow's size and position (rather than
  // applying transformations to this layer).
  ui::Layer* layer() const;

  const gfx::Rect& content_bounds() const { return content_bounds_; }
  Style style() const { return style_; }

  // Moves and resizes |image_grid_| to frame |content_bounds|.
  void SetContentBounds(const gfx::Rect& content_bounds);

  // Sets the shadow's style, animating opacity as necessary.
  void SetStyle(Style style);

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

 private:
  // Updates the |image_grid_| images to the current |style_|.
  void UpdateImagesForStyle();

  // Updates the |image_grid_| bounds based on its image sizes and the
  // current |content_bounds_|.
  void UpdateImageGridBounds();

  // The current style, set when the transition animation starts.
  Style style_;

  scoped_ptr<ImageGrid> image_grid_;

  // Bounds of the content that the shadow encloses.
  gfx::Rect content_bounds_;

  // The interior inset of the shadow images. The content bounds of the image
  // grid should be set to |content_bounds_| inset by this amount.
  int interior_inset_;

  DISALLOW_COPY_AND_ASSIGN(Shadow);
};

}  // namespace wm

#endif  // UI_WM_CORE_SHADOW_H_
