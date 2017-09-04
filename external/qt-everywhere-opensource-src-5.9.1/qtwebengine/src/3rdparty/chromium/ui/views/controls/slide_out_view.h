// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SLIDE_OUT_VIEW_H_
#define UI_VIEWS_CONTROLS_SLIDE_OUT_VIEW_H_

#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

// A View that can be closed by a slide-out touch gesture.
class VIEWS_EXPORT SlideOutView : public views::View,
                                  public ui::ImplicitAnimationObserver {
 public:
  SlideOutView();
  ~SlideOutView() override;

  bool slide_out_enabled() { return is_slide_out_enabled_; }

 protected:
  // Called when user intends to close the View by sliding it out.
  virtual void OnSlideOut() = 0;

  // Overridden from views::View.
  void OnGestureEvent(ui::GestureEvent* event) override;

  void set_slide_out_enabled(bool is_slide_out_enabled) {
    is_slide_out_enabled_ = is_slide_out_enabled;
  }

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
  void OnImplicitAnimationsCompleted() override;

  float gesture_amount_ = 0.f;
  bool is_slide_out_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(SlideOutView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SLIDE_OUT_VIEW_H_
