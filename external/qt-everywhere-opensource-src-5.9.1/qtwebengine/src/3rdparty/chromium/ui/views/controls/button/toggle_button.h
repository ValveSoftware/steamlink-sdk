// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_

#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/custom_button.h"

namespace views {

class Painter;

// This view presents a button that has two states: on and off. This is similar
// to a checkbox but has no text and looks more like a two-state horizontal
// slider.
class VIEWS_EXPORT ToggleButton : public CustomButton {
 public:
  static const char kViewClassName[];

  explicit ToggleButton(ButtonListener* listener);
  ~ToggleButton() override;

  void SetIsOn(bool is_on, bool animate);
  bool is_on() const { return is_on_; }

  void SetFocusPainter(std::unique_ptr<Painter> focus_painter);

  // views::View:
  gfx::Size GetPreferredSize() const override;

 private:
  friend class TestToggleButton;
  class ThumbView;

  // Calculates and returns the bounding box for the track.
  gfx::Rect GetTrackBounds() const;

  // Calculates and returns the bounding box for the thumb (the circle).
  gfx::Rect GetThumbBounds() const;

  // Updates position and color of the thumb.
  void UpdateThumb();

  SkColor GetTrackColor(bool is_on) const;

  // views::View:
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // CustomButton:
  void NotifyClick(const ui::Event& event) override;
  void AddInkDropLayer(ui::Layer* ink_drop_layer) override;
  void RemoveInkDropLayer(ui::Layer* ink_drop_layer) override;
  std::unique_ptr<InkDrop> CreateInkDrop() override;
  std::unique_ptr<InkDropRipple> CreateInkDropRipple() const override;
  SkColor GetInkDropBaseColor() const override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  bool is_on_;
  gfx::SlideAnimation slide_animation_;
  ThumbView* thumb_view_;
  std::unique_ptr<Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(ToggleButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_TOGGLE_BUTTON_H_
