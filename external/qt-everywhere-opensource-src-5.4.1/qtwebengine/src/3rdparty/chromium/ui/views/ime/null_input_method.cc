// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/ime/null_input_method.h"

namespace views {

NullInputMethod::NullInputMethod() {}

void NullInputMethod::SetDelegate(
    internal::InputMethodDelegate* /* delegate */) {}

void NullInputMethod::Init(Widget* /* widget */) {}

void NullInputMethod::OnFocus() {}

void NullInputMethod::OnBlur() {}

bool NullInputMethod::OnUntranslatedIMEMessage(
    const base::NativeEvent& /* event */,
    NativeEventResult* /* result */) {
  return false;
}

void NullInputMethod::DispatchKeyEvent(const ui::KeyEvent& /* key */) {}

void NullInputMethod::OnTextInputTypeChanged(View* /* view */) {}

void NullInputMethod::OnCaretBoundsChanged(View* /* view */) {}

void NullInputMethod::CancelComposition(View* /* view */) {}

void NullInputMethod::OnInputLocaleChanged() {}

std::string NullInputMethod::GetInputLocale() {
  return std::string();
}

bool NullInputMethod::IsActive() {
  return false;
}

ui::TextInputClient* NullInputMethod::GetTextInputClient() const {
  return NULL;
}

ui::TextInputType NullInputMethod::GetTextInputType() const {
  return ui::TEXT_INPUT_TYPE_NONE;
}

bool NullInputMethod::IsCandidatePopupOpen() const {
  return false;
}

void NullInputMethod::ShowImeIfNeeded() {}

bool NullInputMethod::IsMock() const {
  return false;
}

}  // namespace views
