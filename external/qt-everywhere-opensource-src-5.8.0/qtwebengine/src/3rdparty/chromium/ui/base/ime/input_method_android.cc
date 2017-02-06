// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_android.h"

#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

// TODO(bshe): This is currently very similar to InputMethodMUS. Consider unify
// them in the furture if the two have reasonable similarity.

namespace ui {

////////////////////////////////////////////////////////////////////////////////
// InputMethodAndroid, public:

InputMethodAndroid::InputMethodAndroid(
    internal::InputMethodDelegate* delegate) {
  SetDelegate(delegate);
}

InputMethodAndroid::~InputMethodAndroid() {}

bool InputMethodAndroid::OnUntranslatedIMEMessage(
    const base::NativeEvent& event,
    NativeEventResult* result) {
  return false;
}

void InputMethodAndroid::DispatchKeyEvent(ui::KeyEvent* event) {
  DCHECK(event->type() == ui::ET_KEY_PRESSED ||
         event->type() == ui::ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient()) {
    ignore_result(DispatchKeyEventPostIME(event));
    return;
  }

  ignore_result(DispatchKeyEventPostIME(event));
}

void InputMethodAndroid::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
}

void InputMethodAndroid::CancelComposition(
    const ui::TextInputClient* client) {
}

void InputMethodAndroid::OnInputLocaleChanged() {}

std::string InputMethodAndroid::GetInputLocale() {
  return "";
}

bool InputMethodAndroid::IsCandidatePopupOpen() const {
  return false;
}

}  // namespace ui
