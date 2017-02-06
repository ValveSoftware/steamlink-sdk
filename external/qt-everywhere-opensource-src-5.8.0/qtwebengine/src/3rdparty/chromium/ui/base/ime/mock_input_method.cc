// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mock_input_method.h"

#include "ui/base/ime/input_method_delegate.h"
#include "ui/events/event.h"

namespace ui {

MockInputMethod::MockInputMethod(internal::InputMethodDelegate* delegate)
    : text_input_client_(NULL), delegate_(delegate) {
}

MockInputMethod::~MockInputMethod() {
  FOR_EACH_OBSERVER(InputMethodObserver, observer_list_,
                    OnInputMethodDestroyed(this));
}

void MockInputMethod::SetDelegate(internal::InputMethodDelegate* delegate) {
  delegate_ = delegate;
}

void MockInputMethod::SetFocusedTextInputClient(TextInputClient* client) {
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
  return text_input_client_;
}

void MockInputMethod::DispatchKeyEvent(ui::KeyEvent* event) {
  ignore_result(delegate_->DispatchKeyEventPostIME(event));
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

TextInputType MockInputMethod::GetTextInputType() const {
  return TEXT_INPUT_TYPE_NONE;
}

TextInputMode MockInputMethod::GetTextInputMode() const {
  return TEXT_INPUT_MODE_DEFAULT;
}

int MockInputMethod::GetTextInputFlags() const {
  return 0;
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

const std::vector<std::unique_ptr<ui::KeyEvent>>&
MockInputMethod::GetKeyEventsForTesting() {
  return key_events_for_testing_;
}

}  // namespace ui
