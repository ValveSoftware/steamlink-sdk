// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MD_SLIDER_H_
#define UI_VIEWS_CONTROLS_MD_SLIDER_H_

#include "base/macros.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace gfx {
class SlideAnimation;
}

namespace views {

// TODO(yiyix): When material design is enabled by default, use
// MdSlider as the default slider implementation. (crbug.com/614453)
class VIEWS_EXPORT MdSlider : public Slider {
 public:
  explicit MdSlider(SliderListener* listener);
  ~MdSlider() override;

  // ui::Slider:
  void UpdateState(bool control_on) override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

 protected:
  // ui::Slider:
  int GetThumbWidth() override;
  void SetHighlighted(bool is_highlighted) override;

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Record whether the slider is in the active state or the disabled state.
  bool is_active_;

  // Animating value of the current radius of the thumb's highlight.
  float thumb_highlight_radius_;

  std::unique_ptr<gfx::SlideAnimation> highlight_animation_;

  DISALLOW_COPY_AND_ASSIGN(MdSlider);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MD_SLIDER_H_
