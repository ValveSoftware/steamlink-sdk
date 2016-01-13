// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_SLIDER_H_
#define UI_VIEWS_CONTROLS_SLIDER_H_

#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"
#include "ui/views/views_export.h"

typedef unsigned int SkColor;

namespace gfx {
class ImageSkia;
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
  enum Orientation {
    HORIZONTAL,
    VERTICAL
  };

  Slider(SliderListener* listener, Orientation orientation);
  virtual ~Slider();

  float value() const { return value_; }
  void SetValue(float value);

  // Set the delta used for changing the value via keyboard.
  void SetKeyboardIncrement(float increment);

  void SetAccessibleName(const base::string16& name);

  void set_enable_accessibility_events(bool enabled) {
    accessibility_events_enabled_ = enabled;
  }

  void set_focus_border_color(SkColor color) { focus_border_color_ = color; }

  // Update UI based on control on/off state.
  void UpdateState(bool control_on);

 private:
  friend class test::SliderTestApi;

  void SetValueInternal(float value, SliderChangeReason reason);

  // Should be called on the Mouse Down event. Used to calculate relative
  // position of the mouse cursor (or the touch point) on the button to
  // accurately move the button using the MoveButtonTo() method.
  void PrepareForMove(const gfx::Point& point);

  // Moves the button to the specified point and updates the value accordingly.
  void MoveButtonTo(const gfx::Point& point);

  void OnPaintFocus(gfx::Canvas* canvas);

  // Notify the listener_, if not NULL, that dragging started.
  void OnSliderDragStarted();

  // Notify the listener_, if not NULL, that dragging ended.
  void OnSliderDragEnded();

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // gfx::AnimationDelegate overrides:
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE;

  void set_listener(SliderListener* listener) {
    listener_ = listener;
  }

  SliderListener* listener_;
  Orientation orientation_;

  scoped_ptr<gfx::SlideAnimation> move_animation_;

  float value_;
  float keyboard_increment_;
  float animating_value_;
  bool value_is_valid_;
  base::string16 accessible_name_;
  bool accessibility_events_enabled_;
  SkColor focus_border_color_;

  // Relative position of the mouse cursor (or the touch point) on the slider's
  // button.
  gfx::Point initial_button_offset_;

  const int* bar_active_images_;
  const int* bar_disabled_images_;
  const gfx::ImageSkia* thumb_;
  const gfx::ImageSkia* images_[4];
  int bar_height_;

  DISALLOW_COPY_AND_ASSIGN(Slider);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_SLIDER_H_
