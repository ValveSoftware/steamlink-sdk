// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_AURA_GESTURE_NAV_SIMPLE_H_
#define CONTENT_BROWSER_WEB_CONTENTS_AURA_GESTURE_NAV_SIMPLE_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"

namespace gfx {
class Transform;
}

namespace ui {
class Layer;
}

namespace content {

class ArrowLayerDelegate;
class WebContentsImpl;

// A simple delegate for the overscroll controller that paints an arrow on top
// of the web-contents as a hint for pending navigations from overscroll.
class GestureNavSimple : public OverscrollControllerDelegate {
 public:
  explicit GestureNavSimple(WebContentsImpl* web_contents);
  virtual ~GestureNavSimple();

 private:
  void ApplyEffectsAndDestroy(const gfx::Transform& transform, float opacity);
  void AbortGestureAnimation();
  void CompleteGestureAnimation();
  void ApplyEffectsForDelta(float delta_x);

  // OverscrollControllerDelegate:
  virtual gfx::Rect GetVisibleBounds() const OVERRIDE;
  virtual void OnOverscrollUpdate(float delta_x, float delta_y) OVERRIDE;
  virtual void OnOverscrollComplete(OverscrollMode overscroll_mode) OVERRIDE;
  virtual void OnOverscrollModeChange(OverscrollMode old_mode,
                                      OverscrollMode new_mode) OVERRIDE;

  WebContentsImpl* web_contents_;
  scoped_ptr<ui::Layer> clip_layer_;
  scoped_ptr<ui::Layer> arrow_;
  scoped_ptr<ArrowLayerDelegate> arrow_delegate_;
  float completion_threshold_;

  DISALLOW_COPY_AND_ASSIGN(GestureNavSimple);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_AURA_GESTURE_NAV_SIMPLE_H_
