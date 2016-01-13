// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/ime/mock_input_method.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget.h"

namespace views {

MockInputMethod::MockInputMethod()
    : composition_changed_(false),
      focus_changed_(false),
      untranslated_ime_message_called_(false),
      text_input_type_changed_(false),
      caret_bounds_changed_(false),
      cancel_composition_called_(false),
      input_locale_changed_(false),
      locale_("en-US"),
      active_(true) {
}

MockInputMethod::MockInputMethod(internal::InputMethodDelegate* delegate)
    : composition_changed_(false),
      focus_changed_(false),
      untranslated_ime_message_called_(false),
      text_input_type_changed_(false),
      caret_bounds_changed_(false),
      cancel_composition_called_(false),
      input_locale_changed_(false),
      locale_("en-US"),
      active_(true) {
  SetDelegate(delegate);
}

MockInputMethod::~MockInputMethod() {
}

void MockInputMethod::Init(Widget* widget) {
  InputMethodBase::Init(widget);
}

void MockInputMethod::OnFocus() {}

void MockInputMethod::OnBlur() {}

bool MockInputMethod::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  untranslated_ime_message_called_ = true;
  if (result)
    *result = InputMethod::NativeEventResult();
  return false;
}

void MockInputMethod::DispatchKeyEvent(const ui::KeyEvent& key) {
  bool handled = (composition_changed_ || result_text_.length()) &&
      !IsTextInputTypeNone();

  ClearStates();
  if (handled) {
    ui::KeyEvent mock_key(ui::ET_KEY_PRESSED,
                          ui::VKEY_PROCESSKEY,
                          key.flags(),
                          key.is_char());
    DispatchKeyEventPostIME(mock_key);
  } else {
    DispatchKeyEventPostIME(key);
  }

  if (focus_changed_)
    return;

  ui::TextInputClient* client = GetTextInputClient();
  if (client) {
    if (handled) {
      if (result_text_.length())
        client->InsertText(result_text_);
      if (composition_changed_) {
        if (composition_.text.length())
          client->SetCompositionText(composition_);
        else
          client->ClearCompositionText();
      }
    } else if (key.type() == ui::ET_KEY_PRESSED) {
      base::char16 ch = key.GetCharacter();
      client->InsertChar(ch, key.flags());
    }
  }

  ClearResult();
}

void MockInputMethod::OnTextInputTypeChanged(View* view) {
  if (IsViewFocused(view))
    text_input_type_changed_ = true;
  InputMethodBase::OnTextInputTypeChanged(view);
}

void MockInputMethod::OnCaretBoundsChanged(View* view) {
  if (IsViewFocused(view))
    caret_bounds_changed_ = true;
}

void MockInputMethod::CancelComposition(View* view) {
  if (IsViewFocused(view)) {
    cancel_composition_called_ = true;
    ClearResult();
  }
}

void MockInputMethod::OnInputLocaleChanged() {
  input_locale_changed_ = true;
}

std::string MockInputMethod::GetInputLocale() {
  return locale_;
}

bool MockInputMethod::IsActive() {
  return active_;
}

bool MockInputMethod::IsCandidatePopupOpen() const {
  return false;
}

void MockInputMethod::ShowImeIfNeeded() {
}

bool MockInputMethod::IsMock() const {
  return true;
}

void MockInputMethod::OnWillChangeFocus(View* focused_before, View* focused)  {
  ui::TextInputClient* client = GetTextInputClient();
  if (client && client->HasCompositionText())
    client->ConfirmCompositionText();
  focus_changed_ = true;
  ClearResult();
}

void MockInputMethod::Clear() {
  ClearStates();
  ClearResult();
}

void MockInputMethod::SetCompositionTextForNextKey(
    const ui::CompositionText& composition) {
  composition_changed_ = true;
  composition_ = composition;
}

void MockInputMethod::SetResultTextForNextKey(const base::string16& result) {
  result_text_ = result;
}

void MockInputMethod::SetInputLocale(const std::string& locale) {
  if (locale_ != locale) {
    locale_ = locale;
    OnInputMethodChanged();
  }
}

void MockInputMethod::SetActive(bool active) {
  if (active_ != active) {
    active_ = active;
    OnInputMethodChanged();
  }
}

void MockInputMethod::ClearStates() {
  focus_changed_ = false;
  untranslated_ime_message_called_ = false;
  text_input_type_changed_ = false;
  caret_bounds_changed_ = false;
  cancel_composition_called_ = false;
  input_locale_changed_ = false;
}

void MockInputMethod::ClearResult() {
  composition_.Clear();
  composition_changed_ = false;
  result_text_.clear();
}

}  // namespace views
