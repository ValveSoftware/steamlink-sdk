// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/ime/arc_ime_service.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/ime/arc_ime_bridge_impl.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace arc {

ArcImeService::ArcWindowDetector::~ArcWindowDetector() = default;

bool ArcImeService::ArcWindowDetector::IsArcWindow(
    const aura::Window* window) const {
  return exo::Surface::AsSurface(window);
}

bool ArcImeService::ArcWindowDetector::IsArcTopLevelWindow(
    const aura::Window* window) const {
  return exo::ShellSurface::GetMainSurface(window);
}

////////////////////////////////////////////////////////////////////////////////
// ArcImeService main implementation:

ArcImeService::ArcImeService(ArcBridgeService* bridge_service)
    : ArcService(bridge_service),
      ime_bridge_(new ArcImeBridgeImpl(this, bridge_service)),
      arc_window_detector_(new ArcWindowDetector()),
      ime_type_(ui::TEXT_INPUT_TYPE_NONE),
      has_composition_text_(false),
      keyboard_controller_(nullptr),
      test_input_method_(nullptr),
      is_focus_observer_installed_(false) {
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->AddObserver(this);
}

ArcImeService::~ArcImeService() {
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->DetachTextInputClient(this);

  if (is_focus_observer_installed_ && exo::WMHelper::GetInstance())
    exo::WMHelper::GetInstance()->RemoveFocusObserver(this);
  aura::Env* env = aura::Env::GetInstanceDontCreate();
  if (env)
    env->RemoveObserver(this);
  // Removing |this| from KeyboardController.
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller && keyboard_controller_ == keyboard_controller) {
    keyboard_controller_->RemoveObserver(this);
  }
}

void ArcImeService::SetImeBridgeForTesting(
    std::unique_ptr<ArcImeBridge> test_ime_bridge) {
  ime_bridge_ = std::move(test_ime_bridge);
}

void ArcImeService::SetInputMethodForTesting(
    ui::InputMethod* test_input_method) {
  test_input_method_ = test_input_method;
}

void ArcImeService::SetArcWindowDetectorForTesting(
    std::unique_ptr<ArcWindowDetector> detector) {
  arc_window_detector_ = std::move(detector);
}

ui::InputMethod* ArcImeService::GetInputMethod() {
  if (focused_arc_window_.windows().empty())
    return nullptr;

  if (test_input_method_)
    return test_input_method_;
  return focused_arc_window_.windows().front()->GetHost()->GetInputMethod();
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from aura::EnvObserver:

void ArcImeService::OnWindowInitialized(aura::Window* new_window) {
  if (arc_window_detector_->IsArcWindow(new_window)) {
    if (!is_focus_observer_installed_) {
      exo::WMHelper::GetInstance()->AddFocusObserver(this);
      is_focus_observer_installed_ = true;
    }
  }
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (keyboard_controller && keyboard_controller_ != keyboard_controller) {
    // Registering |this| as an observer only once per KeyboardController.
    keyboard_controller_ = keyboard_controller;
    keyboard_controller_->AddObserver(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from exo::WMHelper::FocusChangeObserver:

void ArcImeService::OnWindowFocused(aura::Window* gained_focus,
                                    aura::Window* lost_focus) {
  // The Aura focus may or may not be on sub-window of the toplevel ARC++ frame.
  // To handle all cases, judge the state by always climbing up to the toplevel.
  gained_focus = gained_focus ? gained_focus->GetToplevelWindow() : nullptr;
  lost_focus = lost_focus ? lost_focus->GetToplevelWindow() : nullptr;
  if (lost_focus == gained_focus)
    return;

  const bool detach = (lost_focus && focused_arc_window_.Contains(lost_focus));
  const bool attach =
      (gained_focus && arc_window_detector_->IsArcTopLevelWindow(gained_focus));

  // TODO(kinaba): Implicit dependency in GetInputMethod as described below is
  // confusing. Consider getting InputMethod directly from lost_ or gained_focus
  // variables. For that, we need to change how to inject testing InputMethod.
  //
  // GetInputMethod() retrieves the input method associated to
  // |forcused_arc_window_|. Hence, to get the object we are detaching from, we
  // must call the method before updating the forcused ARC window.
  ui::InputMethod* const detaching_ime = detach ? GetInputMethod() : nullptr;

  if (detach)
    focused_arc_window_.Remove(lost_focus);
  if (attach)
    focused_arc_window_.Add(gained_focus);

  ui::InputMethod* const attaching_ime = attach ? GetInputMethod() : nullptr;

  // Notify to the input method, either when this service is detached or
  // attached. Do nothing when the focus is moving between ARC windows,
  // to avoid unpexpected context reset in ARC.
  if (detaching_ime != attaching_ime) {
    if (detaching_ime)
      detaching_ime->DetachTextInputClient(this);
    if (attaching_ime)
      attaching_ime->SetFocusedTextInputClient(this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from arc::ArcImeBridge::Delegate

void ArcImeService::OnTextInputTypeChanged(ui::TextInputType type) {
  if (ime_type_ == type)
    return;
  ime_type_ = type;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnTextInputTypeChanged(this);
}

void ArcImeService::OnCursorRectChanged(const gfx::Rect& rect) {
  if (cursor_rect_ == rect)
    return;
  cursor_rect_ = rect;

  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->OnCaretBoundsChanged(this);
}

void ArcImeService::OnCancelComposition() {
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method)
    input_method->CancelComposition(this);
}

void ArcImeService::ShowImeIfNeeded() {
  ui::InputMethod* const input_method = GetInputMethod();
  if (input_method && input_method->GetTextInputClient() == this) {
    input_method->ShowImeIfNeeded();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Overridden from keyboard::KeyboardControllerObserver
void ArcImeService::OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) {
  if (focused_arc_window_.windows().empty())
    return;
  aura::Window* window = focused_arc_window_.windows().front();
  // Multiply by the scale factor. To convert from DPI to physical pixels.
  gfx::Rect bounds_in_px = gfx::ScaleToEnclosingRect(
      new_bounds, window->layer()->device_scale_factor());
  ime_bridge_->SendOnKeyboardBoundsChanging(bounds_in_px);
}

void ArcImeService::OnKeyboardClosed() {}

////////////////////////////////////////////////////////////////////////////////
// Overridden from ui::TextInputClient:

void ArcImeService::SetCompositionText(
    const ui::CompositionText& composition) {
  has_composition_text_ = !composition.text.empty();
  ime_bridge_->SendSetCompositionText(composition);
}

void ArcImeService::ConfirmCompositionText() {
  has_composition_text_ = false;
  ime_bridge_->SendConfirmCompositionText();
}

void ArcImeService::ClearCompositionText() {
  if (has_composition_text_) {
    has_composition_text_ = false;
    ime_bridge_->SendInsertText(base::string16());
  }
}

void ArcImeService::InsertText(const base::string16& text) {
  has_composition_text_ = false;
  ime_bridge_->SendInsertText(text);
}

void ArcImeService::InsertChar(const ui::KeyEvent& event) {
  // According to the document in text_input_client.h, InsertChar() is called
  // even when the text input type is NONE. We ignore such events, since for
  // ARC we are only interested in the event as a method of text input.
  if (ime_type_ == ui::TEXT_INPUT_TYPE_NONE)
    return;

  // For apps that doesn't handle hardware keyboard events well, keys that are
  // typically on software keyboard and lack of them are fatal, namely,
  // unmodified enter and backspace keys are sent through IME.
  constexpr int kModifierMask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                                ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN |
                                ui::EF_ALTGR_DOWN | ui::EF_MOD3_DOWN;
  if ((event.flags() & kModifierMask) == 0) {
    if (event.key_code() ==  ui::VKEY_RETURN) {
      has_composition_text_ = false;
      ime_bridge_->SendInsertText(base::ASCIIToUTF16("\n"));
      return;
    }
    if (event.key_code() ==  ui::VKEY_BACK) {
      has_composition_text_ = false;
      ime_bridge_->SendInsertText(base::ASCIIToUTF16("\b"));
      return;
    }
  }

  // Drop 0x00-0x1f (C0 controls), 0x7f (DEL), and 0x80-0x9f (C1 controls).
  // See: https://en.wikipedia.org/wiki/Unicode_control_characters
  // They are control characters and not treated as a text insertion.
  const base::char16 ch = event.GetCharacter();
  const bool is_control_char = (0x00 <= ch && ch <= 0x1f) ||
                               (0x7f <= ch && ch <= 0x9f);

  if (!is_control_char && !ui::IsSystemKeyModifier(event.flags())) {
    has_composition_text_ = false;
    ime_bridge_->SendInsertText(base::string16(1, event.GetText()));
  }
}

ui::TextInputType ArcImeService::GetTextInputType() const {
  return ime_type_;
}

gfx::Rect ArcImeService::GetCaretBounds() const {
  if (focused_arc_window_.windows().empty())
    return gfx::Rect();
  aura::Window* window = focused_arc_window_.windows().front();

  // |cursor_rect_| holds the rectangle reported from ARC apps, in the "screen
  // coordinates" in ARC, counted by physical pixels.
  // Chrome OS input methods expect the coordinates in Chrome OS screen, within
  // device independent pixels. Two factors are involved for the conversion.

  // Divide by the scale factor. To convert from physical pixels to DIP.
  gfx::Rect converted = gfx::ScaleToEnclosingRect(
      cursor_rect_, 1 / window->layer()->device_scale_factor());

  // Add the offset of the window showing the ARC app.
  converted.Offset(window->GetBoundsInScreen().OffsetFromOrigin());
  return converted;
}

ui::TextInputMode ArcImeService::GetTextInputMode() const {
  return ui::TEXT_INPUT_MODE_DEFAULT;
}

base::i18n::TextDirection ArcImeService::GetTextDirection() const {
  return base::i18n::UNKNOWN_DIRECTION;
}

void ArcImeService::ExtendSelectionAndDelete(size_t before, size_t after) {
  ime_bridge_->SendExtendSelectionAndDelete(before, after);
}

int ArcImeService::GetTextInputFlags() const {
  return ui::TEXT_INPUT_FLAG_NONE;
}

bool ArcImeService::CanComposeInline() const {
  return true;
}

bool ArcImeService::GetCompositionCharacterBounds(
    uint32_t index, gfx::Rect* rect) const {
  return false;
}

bool ArcImeService::HasCompositionText() const {
  return has_composition_text_;
}

bool ArcImeService::GetTextRange(gfx::Range* range) const {
  return false;
}

bool ArcImeService::GetCompositionTextRange(gfx::Range* range) const {
  return false;
}

bool ArcImeService::GetSelectionRange(gfx::Range* range) const {
  return false;
}

bool ArcImeService::SetSelectionRange(const gfx::Range& range) {
  return false;
}

bool ArcImeService::DeleteRange(const gfx::Range& range) {
  return false;
}

bool ArcImeService::GetTextFromRange(
    const gfx::Range& range, base::string16* text) const {
  return false;
}

bool ArcImeService::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return false;
}

bool ArcImeService::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  return false;
}

}  // namespace arc
