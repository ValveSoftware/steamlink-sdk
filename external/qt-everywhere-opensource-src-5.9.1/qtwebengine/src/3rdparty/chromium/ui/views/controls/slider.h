// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SLIDER_H_
#define UI_VIEWS_CONTROLS_SLIDER_H_

#include "base/macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

typedef unsigned int SkColor;

namespace gfx {
class SlideAnimation;
}

namespace views {

namespace test {
class SliderTestApi;
}

class Slider;

enum SliderChangeReason {
  VALUE_CHANGED_BY_USER,  // value was changed by the user (by clicking, e.g.)
  VALUE_CHANGED_BY_API,   // value was changed by a call to SetValue.
};

class VIEWS_EXPORT SliderListener {
 public:
  virtual void SliderValueChanged(Slider* sender,
                                  float value,
                                  float old_value,
                                  SliderChangeReason reason) = 0;

  // Invoked when a drag starts or ends (more specifically, when the mouse
  // button is pressed or released).
  virtual void SliderDragStarted(Slider* sender) {}
  virtual void SliderDragEnded(Slider* sender) {}

 protected:
  virtual ~SliderListener() {}
};

class VIEWS_EXPORT Slider : public View, public gfx::AnimationDelegate {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // Based on the bool |is_material_design|, either a md version or a non-md
  // version of the slider will be created.
  static Slider* CreateSlider(bool is_material_design,
                              SliderListener* listener);
  ~Slider() override;

  float value() const { return value_; }
  void SetValue(float value);

  void SetAccessibleName(const base::string16& name);

  void set_enable_accessibility_events(bool enabled) {
    accessibility_events_enabled_ = enabled;
  }

  void set_focus_border_color(SkColor color) { focus_border_color_ = color; }

  // Update UI based on control on/off state.
  virtual void UpdateState(bool control_on) = 0;

 protected:
  explicit Slider(SliderListener* listener);

  // Returns the current position of the thumb on the slider.
  float GetAnimatingValue() const;

  // Shows or hides the highlight on the slider thumb. The default
  // implementation does nothing.
  virtual void SetHighlighted(bool is_highlighted);

  // Gets the size of the slider's thumb.
  virtual int GetThumbWidth() = 0;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  friend class test::SliderTestApi;

  void SetValueInternal(float value, SliderChangeReason reason);

  // Should be called on the Mouse Down event. Used to calculate relative
  // position of the mouse cursor (or the touch point) on the button to
  // accurately move the button using the MoveButtonTo() method.
  void PrepareForMove(const int new_x);

  // Moves the button to the specified point and updates the value accordingly.
  void MoveButtonTo(const gfx::Point& point);

  void OnPaintFocus(gfx::Canvas* canvas);

  // Notify the listener_, if not NULL, that dragging started.
  void OnSliderDragStarted();

  // Notify the listener_, if not NULL, that dragging ended.
  void OnSliderDragEnded();

  // views::View:
  const char* GetClassName() const override;
  gfx::Size GetPreferredSize() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnFocus() override;
  void OnBlur() override;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  void set_listener(SliderListener* listener) {
    listener_ = listener;
  }

  SliderListener* listener_;

  std::unique_ptr<gfx::SlideAnimation> move_animation_;

  float value_;
  float keyboard_increment_;
  float initial_animating_value_;
  bool value_is_valid_;
  base::string16 accessible_name_;
  bool accessibility_events_enabled_;
  SkColor focus_border_color_;

  // Relative position of the mouse cursor (or the touch point) on the slider's
  // button.
  int initial_button_offset_;

  DISALLOW_COPY_AND_ASSIGN(Slider);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SLIDER_H_
