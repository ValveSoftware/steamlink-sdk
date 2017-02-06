// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/button.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_view_state.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Button, static public:

// static
Button::ButtonState Button::GetButtonStateFrom(ui::NativeTheme::State state) {
  switch (state) {
    case ui::NativeTheme::kDisabled:  return Button::STATE_DISABLED;
    case ui::NativeTheme::kHovered:   return Button::STATE_HOVERED;
    case ui::NativeTheme::kNormal:    return Button::STATE_NORMAL;
    case ui::NativeTheme::kPressed:   return Button::STATE_PRESSED;
    case ui::NativeTheme::kNumStates: NOTREACHED();
  }
  return Button::STATE_NORMAL;
}

////////////////////////////////////////////////////////////////////////////////
// Button, public:

Button::~Button() {
}

void Button::SetFocusForPlatform() {
#if defined(OS_MACOSX)
  // On Mac, buttons are focusable only in full keyboard access mode.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
  SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
}

void Button::SetTooltipText(const base::string16& tooltip_text) {
  tooltip_text_ = tooltip_text;
  if (accessible_name_.empty())
    accessible_name_ = tooltip_text_;
  TooltipTextChanged();
}

void Button::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

////////////////////////////////////////////////////////////////////////////////
// Button, View overrides:

bool Button::GetTooltipText(const gfx::Point& p,
                            base::string16* tooltip) const {
  if (tooltip_text_.empty())
    return false;

  *tooltip = tooltip_text_;
  return true;
}

void Button::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = accessible_name_;
}

////////////////////////////////////////////////////////////////////////////////
// Button, protected:

Button::Button(ButtonListener* listener)
    : listener_(listener),
      tag_(-1) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
}

void Button::NotifyClick(const ui::Event& event) {
  // We can be called when there is no listener, in cases like double clicks on
  // menu buttons etc.
  if (listener_)
    listener_->ButtonPressed(this, event);
}

void Button::OnClickCanceled(const ui::Event& event) {}

}  // namespace views
