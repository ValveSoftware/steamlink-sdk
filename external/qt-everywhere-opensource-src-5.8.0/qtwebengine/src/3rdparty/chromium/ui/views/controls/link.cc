// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/link.h"

#include "build/build_config.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/native_cursor.h"

namespace views {

const char Link::kViewClassName[] = "Link";

Link::Link() : Link(base::string16()) {}

Link::Link(const base::string16& title)
    : Label(title),
      requested_enabled_color_(gfx::kPlaceholderColor),
      requested_enabled_color_set_(false),
      requested_pressed_color_(gfx::kPlaceholderColor),
      requested_pressed_color_set_(false) {
  Init();
}

Link::~Link() {
}

const char* Link::GetClassName() const {
  return kViewClassName;
}

gfx::NativeCursor Link::GetCursor(const ui::MouseEvent& event) {
  if (!enabled())
    return gfx::kNullCursor;
  return GetNativeHandCursor();
}

bool Link::CanProcessEventsWithinSubtree() const {
  // Links need to be able to accept events (e.g., clicking) even though
  // in general Labels do not.
  return View::CanProcessEventsWithinSubtree();
}

bool Link::OnMousePressed(const ui::MouseEvent& event) {
  if (!enabled() ||
      (!event.IsLeftMouseButton() && !event.IsMiddleMouseButton()))
    return false;
  SetPressed(true);
  return true;
}

bool Link::OnMouseDragged(const ui::MouseEvent& event) {
  SetPressed(enabled() &&
             (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) &&
             HitTestPoint(event.location()));
  return true;
}

void Link::OnMouseReleased(const ui::MouseEvent& event) {
  // Change the highlight first just in case this instance is deleted
  // while calling the controller
  OnMouseCaptureLost();
  if (enabled() &&
      (event.IsLeftMouseButton() || event.IsMiddleMouseButton()) &&
      HitTestPoint(event.location())) {
    // Focus the link on click.
    RequestFocus();

    if (listener_)
      listener_->LinkClicked(this, event.flags());
  }
}

void Link::OnMouseCaptureLost() {
  SetPressed(false);
}

bool Link::OnKeyPressed(const ui::KeyEvent& event) {
  bool activate = (((event.key_code() == ui::VKEY_SPACE) &&
                    (event.flags() & ui::EF_ALT_DOWN) == 0) ||
                   (event.key_code() == ui::VKEY_RETURN));
  if (!activate)
    return false;

  SetPressed(false);

  // Focus the link on key pressed.
  RequestFocus();

  if (listener_)
    listener_->LinkClicked(this, event.flags());

  return true;
}

void Link::OnGestureEvent(ui::GestureEvent* event) {
  if (!enabled())
    return;

  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    SetPressed(true);
  } else if (event->type() == ui::ET_GESTURE_TAP) {
    RequestFocus();
    if (listener_)
      listener_->LinkClicked(this, event->flags());
  } else {
    SetPressed(false);
    return;
  }
  event->SetHandled();
}

bool Link::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // Make sure we don't process space or enter as accelerators.
  return (event.key_code() == ui::VKEY_SPACE) ||
      (event.key_code() == ui::VKEY_RETURN);
}

void Link::GetAccessibleState(ui::AXViewState* state) {
  Label::GetAccessibleState(state);
  state->role = ui::AX_ROLE_LINK;
}

void Link::OnEnabledChanged() {
  RecalculateFont();
  View::OnEnabledChanged();
}

void Link::OnFocus() {
  Label::OnFocus();
  RecalculateFont();
  // We render differently focused.
  SchedulePaint();
}

void Link::OnBlur() {
  Label::OnBlur();
  RecalculateFont();
  // We render differently focused.
  SchedulePaint();
}

void Link::SetFontList(const gfx::FontList& font_list) {
  Label::SetFontList(font_list);
  RecalculateFont();
}

void Link::SetText(const base::string16& text) {
  Label::SetText(text);
  ConfigureFocus();
}

void Link::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  Label::SetEnabledColor(GetEnabledColor());
  SetDisabledColor(
      theme->GetSystemColor(ui::NativeTheme::kColorId_LinkDisabled));
}

void Link::SetEnabledColor(SkColor color) {
  requested_enabled_color_set_ = true;
  requested_enabled_color_ = color;
  Label::SetEnabledColor(GetEnabledColor());
}

void Link::SetPressedColor(SkColor color) {
  requested_pressed_color_set_ = true;
  requested_pressed_color_ = color;
  Label::SetEnabledColor(GetEnabledColor());
}

void Link::SetUnderline(bool underline) {
  if (underline_ == underline)
    return;
  underline_ = underline;
  RecalculateFont();
}

void Link::Init() {
  listener_ = NULL;
  pressed_ = false;
  underline_ = !ui::MaterialDesignController::IsSecondaryUiMaterial();
  RecalculateFont();

  // Label::Init() calls SetText(), but if that's being called from Label(), our
  // SetText() override will not be reached (because the constructed class is
  // only a Label at the moment, not yet a Link).  So explicitly configure focus
  // here.
  ConfigureFocus();
}

void Link::SetPressed(bool pressed) {
  if (pressed_ != pressed) {
    pressed_ = pressed;
    Label::SetEnabledColor(GetEnabledColor());
    RecalculateFont();
    SchedulePaint();
  }
}

void Link::RecalculateFont() {
  // Underline the link if it is enabled and |underline_| is true. Also
  // underline to indicate focus in MD.
  const int style = font_list().GetFontStyle();
  const bool underline =
      underline_ ||
      (HasFocus() && ui::MaterialDesignController::IsSecondaryUiMaterial());
  const int intended_style = (enabled() && underline) ?
      (style | gfx::Font::UNDERLINE) : (style & ~gfx::Font::UNDERLINE);

  if (style != intended_style)
    Label::SetFontList(font_list().DeriveWithStyle(intended_style));
}

void Link::ConfigureFocus() {
  // Disable focusability for empty links.  Otherwise Label::GetInsets() will
  // give them an unconditional 1-px. inset on every side to allow for a focus
  // border, when in this case we probably wanted zero width.
  if (text().empty()) {
    SetFocusBehavior(FocusBehavior::NEVER);
  } else {
#if defined(OS_MACOSX)
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
#else
    SetFocusBehavior(FocusBehavior::ALWAYS);
#endif
  }
}

SkColor Link::GetEnabledColor() {
  // In material mode, there is no pressed effect, so always use the unpressed
  // color.
  if (!pressed_ || ui::MaterialDesignController::IsModeMaterial()) {
    if (!requested_enabled_color_set_ && GetNativeTheme())
      return GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_LinkEnabled);

    return requested_enabled_color_;
  }

  if (!requested_pressed_color_set_ && GetNativeTheme())
    return GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_LinkPressed);

  return requested_pressed_color_;
}

}  // namespace views
