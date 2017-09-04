// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/slider.h"

#include <algorithm>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/md_slider.h"
#include "ui/views/controls/non_md_slider.h"
#include "ui/views/resources/grit/views_resources.h"
#include "ui/views/widget/widget.h"

namespace {
const int kSlideValueChangeDurationMs = 150;

// The image chunks.
enum BorderElements {
  LEFT,
  CENTER_LEFT,
  CENTER_RIGHT,
  RIGHT,
};
}  // namespace

namespace views {

// static
const char Slider::kViewClassName[] = "Slider";

// static
Slider* Slider::CreateSlider(bool is_material_design,
                             SliderListener* listener) {
  if (is_material_design)
    return new MdSlider(listener);
  return new NonMdSlider(listener);
}

Slider::~Slider() {
}

void Slider::SetValue(float value) {
  SetValueInternal(value, VALUE_CHANGED_BY_API);
}

void Slider::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

Slider::Slider(SliderListener* listener)
    : listener_(listener),
      value_(0.f),
      keyboard_increment_(0.1f),
      initial_animating_value_(0.f),
      value_is_valid_(false),
      accessibility_events_enabled_(true),
      focus_border_color_(0),
      initial_button_offset_(0) {
  EnableCanvasFlippingForRTLUI(true);
#if defined(OS_MACOSX)
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
}

float Slider::GetAnimatingValue() const{
  return move_animation_ && move_animation_->is_animating()
             ? move_animation_->CurrentValueBetween(initial_animating_value_,
                                                    value_)
             : value_;
}

void Slider::SetHighlighted(bool is_highlighted) {}

void Slider::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  OnPaintFocus(canvas);
}

void Slider::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == move_animation_.get())
    SchedulePaint();
}

void Slider::AnimationEnded(const gfx::Animation* animation) {
  if (animation == move_animation_.get())
    move_animation_.reset();
}

void Slider::SetValueInternal(float value, SliderChangeReason reason) {
  bool old_value_valid = value_is_valid_;

  value_is_valid_ = true;
  if (value < 0.0)
    value = 0.0;
  else if (value > 1.0)
    value = 1.0;
  if (value_ == value)
    return;
  float old_value = value_;
  value_ = value;
  if (listener_)
    listener_->SliderValueChanged(this, value_, old_value, reason);

  if (old_value_valid && base::MessageLoop::current()) {
    // Do not animate when setting the value of the slider for the first time.
    // There is no message-loop when running tests. So we cannot animate then.
    if (!move_animation_) {
      initial_animating_value_ = old_value;
      move_animation_.reset(new gfx::SlideAnimation(this));
      move_animation_->SetSlideDuration(kSlideValueChangeDurationMs);
      move_animation_->Show();
    }
  } else {
    SchedulePaint();
  }
  if (accessibility_events_enabled_ && GetWidget())
    NotifyAccessibilityEvent(ui::AX_EVENT_VALUE_CHANGED, true);
}

void Slider::PrepareForMove(const int new_x) {
  // Try to remember the position of the mouse cursor on the button.
  gfx::Insets inset = GetInsets();
  gfx::Rect content = GetContentsBounds();
  float value = GetAnimatingValue();

  const int thumb_width = GetThumbWidth();
  const int thumb_x = value * (content.width() - thumb_width);
  const int candidate_x = (base::i18n::IsRTL() ?
      width() - (new_x - inset.left()) :
      new_x - inset.left()) - thumb_x;
  if (candidate_x >= 0 && candidate_x < thumb_width)
    initial_button_offset_ = candidate_x;
  else
    initial_button_offset_ = thumb_width / 2;
}

void Slider::MoveButtonTo(const gfx::Point& point) {
  const gfx::Insets inset = GetInsets();
  const int thumb_width = GetThumbWidth();
  // Calculate the value.
  int amount = base::i18n::IsRTL()
                   ? width() - inset.left() - point.x() - initial_button_offset_
                   : point.x() - inset.left() - initial_button_offset_;
  SetValueInternal(
      static_cast<float>(amount) / (width() - inset.width() - thumb_width),
      VALUE_CHANGED_BY_USER);
}

void Slider::OnPaintFocus(gfx::Canvas* canvas) {
  if (!HasFocus())
    return;

  if (!focus_border_color_) {
    canvas->DrawFocusRect(GetLocalBounds());
  } else if (HasFocus()) {
    canvas->DrawSolidFocusRect(
        gfx::Rect(1, 1, width() - 3, height() - 3),
        focus_border_color_);
  }
}

void Slider::OnSliderDragStarted() {
  SetHighlighted(true);
  if (listener_)
    listener_->SliderDragStarted(this);
}

void Slider::OnSliderDragEnded() {
  SetHighlighted(false);
  if (listener_)
    listener_->SliderDragEnded(this);
}

const char* Slider::GetClassName() const {
  return kViewClassName;
}

gfx::Size Slider::GetPreferredSize() const {
  const int kSizeMajor = 200;
  const int kSizeMinor = 40;

  return gfx::Size(std::max(width(), kSizeMajor), kSizeMinor);
}

bool Slider::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;
  OnSliderDragStarted();
  PrepareForMove(event.location().x());
  MoveButtonTo(event.location());
  return true;
}

bool Slider::OnMouseDragged(const ui::MouseEvent& event) {
  MoveButtonTo(event.location());
  return true;
}

void Slider::OnMouseReleased(const ui::MouseEvent& event) {
  OnSliderDragEnded();
}

bool Slider::OnKeyPressed(const ui::KeyEvent& event) {
  float new_value = value_;
  if (event.key_code() == ui::VKEY_LEFT)
    new_value -= keyboard_increment_;
  else if (event.key_code() == ui::VKEY_RIGHT)
    new_value += keyboard_increment_;
  else
    return false;
  SetValueInternal(new_value, VALUE_CHANGED_BY_USER);
  return true;
}

void Slider::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_SLIDER;
  node_data->SetName(accessible_name_);
  node_data->SetValue(base::UTF8ToUTF16(
      base::StringPrintf("%d%%", static_cast<int>(value_ * 100 + 0.5))));
}

void Slider::OnFocus() {
  View::OnFocus();
  SchedulePaint();
}

void Slider::OnBlur() {
  View::OnBlur();
  SchedulePaint();
}

void Slider::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    // In a multi point gesture only the touch point will generate
    // an ET_GESTURE_TAP_DOWN event.
    case ui::ET_GESTURE_TAP_DOWN:
      OnSliderDragStarted();
      PrepareForMove(event->location().x());
      // Intentional fall through to next case.
    case ui::ET_GESTURE_SCROLL_BEGIN:
    case ui::ET_GESTURE_SCROLL_UPDATE:
      MoveButtonTo(event->location());
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      MoveButtonTo(event->location());
      event->SetHandled();
      if (event->details().touch_points() <= 1)
        OnSliderDragEnded();
      break;
    default:
      break;
  }
}

}  // namespace views
