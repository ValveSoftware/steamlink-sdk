// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mock_input_method.h"

#include "ui/base/ime/text_input_focus_manager.h"
#include "ui/base/ui_base_switches_util.h"

namespace ui {

MockInputMethod::MockInputMethod(internal::InputMethodDelegate* delegate)
    : text_input_client_(NULL) {
}

MockInputMethod::~MockInputMethod() {
}

void MockInputMethod::SetDelegate(internal::InputMethodDelegate* delegate) {
}

void MockInputMethod::SetFocusedTextInputClient(TextInputClient* client) {
  if (switches::IsTextInputFocusManagerEnabled())
    return;

  if (text_input_client_ == client)
    return;
  text_input_client_ = client;
  if (client)
    OnTextInputTypeChanged(client);
}

void MockInputMethod::DetachTextInputClient(TextInputClient* client) {
  if (text_input_client_ == client) {
    text_input_client_ = NULL;
  }
}

TextInputClient* MockInputMethod::GetTextInputClient() const {
  if (switches::IsTextInputFocusManagerEnabled())
    return TextInputFocusManager::GetInstance()->GetFocusedTextInputClient();

  return text_input_client_;
}

bool MockInputMethod::DispatchKeyEvent(const ui::KeyEvent& event) {
  return false;
}

void MockInputMethod::Init(bool focused) {
}

void MockInputMethod::OnFocus() {
  FOR_EACH_OBSERVER(InputMethodObserver, observer_list_, OnFocus());
}

void MockInputMethod::OnBlur() {
  FOR_EACH_OBSERVER(InputMethodObserver, observer_list_, OnBlur());
}

bool MockInputMethod::OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                               NativeEventResult* result) {
  if (result)
    *result = NativeEventResult();
  return false;
}

void MockInputMethod::OnTextInputTypeChanged(const TextInputClient* client) {
  FOR_EACH_OBSERVER(InputMethodObserver,
                    observer_list_,
                    OnTextInputTypeChanged(client));
  FOR_EACH_OBSERVER(InputMethodObserver,
                    observer_list_,
                    OnTextInputStateChanged(client));
}

void MockInputMethod::OnCaretBoundsChanged(const TextInputClient* client) {
  FOR_EACH_OBSERVER(InputMethodObserver,
                    observer_list_,
                    OnCaretBoundsChanged(client));
}

void MockInputMethod::CancelComposition(const TextInputClient* client) {
}

void MockInputMethod::OnInputLocaleChanged() {
}

std::string MockInputMethod::GetInputLocale() {
  return "";
}

bool MockInputMethod::IsActive() {
  return true;
}

TextInputType MockInputMethod::GetTextInputType() const {
  return TEXT_INPUT_TYPE_NONE;
}

TextInputMode MockInputMethod::GetTextInputMode() const {
  return TEXT_INPUT_MODE_DEFAULT;
}

bool MockInputMethod::CanComposeInline() const {
  return true;
}

bool MockInputMethod::IsCandidatePopupOpen() const {
  return false;
}

void MockInputMethod::ShowImeIfNeeded() {
  FOR_EACH_OBSERVER(InputMethodObserver, observer_list_, OnShowImeIfNeeded());
}

void MockInputMethod::AddObserver(InputMethodObserver* observer) {
  observer_list_.AddObserver(observer);
}

void MockInputMethod::RemoveObserver(InputMethodObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace ui
