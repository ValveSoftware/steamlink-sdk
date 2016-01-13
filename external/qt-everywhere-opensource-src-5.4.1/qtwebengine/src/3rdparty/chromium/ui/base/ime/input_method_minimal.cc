// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_minimal.h"

#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ui {

InputMethodMinimal::InputMethodMinimal(
    internal::InputMethodDelegate* delegate) {
  SetDelegate(delegate);
}

InputMethodMinimal::~InputMethodMinimal() {}

bool InputMethodMinimal::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  return false;
}

bool InputMethodMinimal::DispatchKeyEvent(const ui::KeyEvent& event) {
  DCHECK(event.type() == ET_KEY_PRESSED || event.type() == ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient())
    return DispatchKeyEventPostIME(event);

  // Insert the character.
  const bool handled = DispatchKeyEventPostIME(event);
  if (event.type() == ET_KEY_PRESSED && GetTextInputClient()) {
    const uint16 ch = event.GetCharacter();
    if (ch) {
      GetTextInputClient()->InsertChar(ch, event.flags());
      return true;
    }
  }
  return handled;
}

void InputMethodMinimal::OnCaretBoundsChanged(const TextInputClient* client) {}

void InputMethodMinimal::CancelComposition(const TextInputClient* client) {}

void InputMethodMinimal::OnInputLocaleChanged() {}

std::string InputMethodMinimal::GetInputLocale() {
  return std::string();
}

bool InputMethodMinimal::IsActive() {
  return true;
}

bool InputMethodMinimal::IsCandidatePopupOpen() const {
  return false;
}

}  // namespace ui
