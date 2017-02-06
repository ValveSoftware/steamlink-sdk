// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_mac.h"

namespace ui {

InputMethodMac::InputMethodMac(internal::InputMethodDelegate* delegate) {
  SetDelegate(delegate);
}

InputMethodMac::~InputMethodMac() {
}

bool InputMethodMac::OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                              NativeEventResult* result) {
  return false;
}

void InputMethodMac::DispatchKeyEvent(ui::KeyEvent* event) {
  // This is used on Mac only to dispatch events post-IME.
  ignore_result(DispatchKeyEventPostIME(event));
}

void InputMethodMac::OnCaretBoundsChanged(const TextInputClient* client) {
}

void InputMethodMac::CancelComposition(const TextInputClient* client) {
}

void InputMethodMac::OnInputLocaleChanged() {
}

std::string InputMethodMac::GetInputLocale() {
  return "";
}

bool InputMethodMac::IsCandidatePopupOpen() const {
  // There seems to be no way to tell if a candidate window is open.
  return false;
}

}  // namespace ui
