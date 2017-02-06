// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// The delegate receives overscroll gesture updates from the controller and
// should perform appropriate actions.
class CONTENT_EXPORT OverscrollControllerDelegate {
 public:
  OverscrollControllerDelegate() {}
  virtual ~OverscrollControllerDelegate() {}

  // Get the bounds of the view corresponding to the delegate. Overscroll-ending
  // events will only be processed if the visible bounds are non-empty.
  virtual gfx::Rect GetVisibleBounds() const = 0;

  // This is called for each update in the overscroll amount. Returns true if
  // the delegate consumed the event.
  virtual bool OnOverscrollUpdate(float delta_x, float delta_y) = 0;

  // This is called when the overscroll completes.
  virtual void OnOverscrollComplete(OverscrollMode overscroll_mode) = 0;

  // This is called when the direction of the overscroll changes.
  virtual void OnOverscrollModeChange(OverscrollMode old_mode,
                                      OverscrollMode new_mode) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverscrollControllerDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_OVERSCROLL_CONTROLLER_DELEGATE_H_
