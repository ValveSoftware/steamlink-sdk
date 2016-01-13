// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SLIDE_OUT_VIEW_H_
#define UI_VIEWS_CONTROLS_SLIDE_OUT_VIEW_H_

#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

// A View that can be closed by a slide-out touch gesture.
class VIEWS_EXPORT SlideOutView : public views::View,
                                  public ui::ImplicitAnimationObserver {
 public:
  SlideOutView();
  virtual ~SlideOutView();

 protected:
  // Called when user intends to close the View by sliding it out.
  virtual void OnSlideOut() = 0;

  // Overridden from views::View.
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  enum SlideDirection {
    SLIDE_LEFT,
    SLIDE_RIGHT,
  };

  // Restores the transform and opacity of the view.
  void RestoreVisualState();

  // Slides the view out and closes it after the animation.
  void SlideOutAndClose(SlideDirection direction);

  // Overridden from ImplicitAnimationObserver.
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  float gesture_scroll_amount_;

  DISALLOW_COPY_AND_ASSIGN(SlideOutView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SLIDE_OUT_VIEW_H_
