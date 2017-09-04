// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_NON_MD_SLIDER_H_
#define UI_VIEWS_CONTROLS_NON_MD_SLIDER_H_

#include "base/macros.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {
class Slider;

class VIEWS_EXPORT NonMdSlider : public Slider {
 public:
  explicit NonMdSlider(SliderListener* listener);
  ~NonMdSlider() override;

  // ui::Slider:
  void UpdateState(bool control_on) override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

 protected:
  // ui::Slider:
  int GetThumbWidth() override;

 private:
  void UpdateSliderAppearance(bool control_on);

  // Array used to hold active slider states and disabled slider states images.
  const int* bar_active_images_;
  const int* bar_disabled_images_;
  const gfx::ImageSkia* thumb_;

  // Array used to hold current state of slider images.
  const gfx::ImageSkia* images_[4];
  int bar_height_;

  DISALLOW_COPY_AND_ASSIGN(NonMdSlider);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_NON_MD_SLIDER_H_
