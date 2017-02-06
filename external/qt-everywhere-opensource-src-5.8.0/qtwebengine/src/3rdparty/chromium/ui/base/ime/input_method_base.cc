// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

namespace ui {

InputMethodBase::InputMethodBase()
    : sending_key_event_(false),
      delegate_(nullptr),
      text_input_client_(nullptr) {}

InputMethodBase::~InputMethodBase() {
  FOR_EACH_OBSERVER(InputMethodObserver,
                    observer_list_,
                    OnInputMethodDestroyed(this));
  if (ui::IMEBridge::Get() &&
      ui::IMEBridge::Get()->GetInputContextHandler() == this)
    ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
}

void InputMethodBase::SetDelegate(internal::InputMethodDelegate* delegate) {
  delegate_ = delegate;
}

void InputMethodBase::OnFocus() {
  if (ui::IMEBridge::Get())
    ui::IMEBridge::Get()->SetInputContextHandler(this);
}

void InputMethodBase::OnBlur() {
  if (ui::IMEBridge::Get() &&
      ui::IMEBridge::Get()->GetInputContextHandler() == this)
    ui::IMEBridge::Get()->SetInputContextHandler(nullptr);
}

void InputMethodBase::SetFocusedTextInputClient(TextInputClient* client) {
  SetFocusedTextInputClientInternal(client);
}

void InputMethodBase::DetachTextInputClient(TextInputClient* client) {
  if (text_input_client_ != client)
    return;
  SetFocusedTextInputClientInternal(nullptr);
}

TextInputClient* InputMethodBase::GetTextInputClient() const {
  return text_input_client_;
}

void InputMethodBase::OnTextInputTypeChanged(const TextInputClient* client) {
  if (!IsTextInputClientFocused(client))
    return;
  NotifyTextInputStateChanged(client);
}

TextInputType InputMethodBase::GetTextInputType() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputType() : TEXT_INPUT_TYPE_NONE;
}

TextInputMode InputMethodBase::GetTextInputMode() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputMode() : TEXT_INPUT_MODE_DEFAULT;
}

int InputMethodBase::GetTextInputFlags() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->GetTextInputFlags() : 0;
}

bool InputMethodBase::CanComposeInline() const {
  TextInputClient* client = GetTextInputClient();
  return client ? client->CanComposeInline() : true;
}

void InputMethodBase::ShowImeIfNeeded() {
  FOR_EACH_OBSERVER(InputMethodObserver, observer_list_, OnShowImeIfNeeded());
}

void InputMethodBase::AddObserver(InputMethodObserver* observer) {
  observer_list_.AddObserver(observer);
}

void InputMethodBase::RemoveObserver(InputMethodObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool InputMethodBase::IsTextInputClientFocused(const TextInputClient* client) {
  return client && (client == GetTextInputClient());
}

bool InputMethodBase::IsTextInputTypeNone() const {
  return GetTextInputType() == TEXT_INPUT_TYPE_NONE;
}

void InputMethodBase::OnInputMethodChanged() const {
  TextInputClient* client = GetTextInputClient();
  if (!IsTextInputTypeNone())
    client->OnInputMethodChanged();
}

ui::EventDispatchDetails InputMethodBase::DispatchKeyEventPostIME(
    ui::KeyEvent* event) const {
  ui::EventDispatchDetails details;
  if (delegate_)
    details = delegate_->DispatchKeyEventPostIME(event);
  return details;
}

void InputMethodBase::NotifyTextInputStateChanged(
    const TextInputClient* client) {
  FOR_EACH_OBSERVER(InputMethodObserver,
                    observer_list_,
                    OnTextInputStateChanged(client));
}

void InputMethodBase::NotifyTextInputCaretBoundsChanged(
    const TextInputClient* client) {
  FOR_EACH_OBSERVER(
      InputMethodObserver, observer_list_, OnCaretBoundsChanged(client));
}

void InputMethodBase::SetFocusedTextInputClientInternal(
    TextInputClient* client) {
  TextInputClient* old = text_input_client_;
  if (old == client)
    return;
  OnWillChangeFocusedClient(old, client);
  text_input_client_ = client;  // nullptr allowed.
  OnDidChangeFocusedClient(old, client);
  NotifyTextInputStateChanged(text_input_client_);
}

std::vector<gfx::Rect> InputMethodBase::GetCompositionBounds(
    const TextInputClient* client) {
  std::vector<gfx::Rect> bounds;
  if (client->HasCompositionText()) {
    uint32_t i = 0;
    gfx::Rect rect;
    while (client->GetCompositionCharacterBounds(i++, &rect))
      bounds.push_back(rect);
  } else {
    // For case of no composition at present, use caret bounds which is required
    // by the IME extension for certain features (e.g. physical keyboard
    // auto-correct).
    bounds.push_back(client->GetCaretBounds());
  }
  return bounds;
}

bool InputMethodBase::SendFakeProcessKeyEvent(bool pressed) const {
  KeyEvent evt(pressed ? ET_KEY_PRESSED : ET_KEY_RELEASED,
               pressed ? VKEY_PROCESSKEY : VKEY_UNKNOWN, EF_IME_FABRICATED_KEY);
  ignore_result(DispatchKeyEventPostIME(&evt));
  return evt.stopped_propagation();
}

void InputMethodBase::CommitText(const std::string& text) {
  if (text.empty() || !GetTextInputClient() || IsTextInputTypeNone())
    return;

  const base::string16 utf16_text = base::UTF8ToUTF16(text);
  if (utf16_text.empty())
    return;

  if (!SendFakeProcessKeyEvent(true))
    GetTextInputClient()->InsertText(utf16_text);
  SendFakeProcessKeyEvent(false);
}

void InputMethodBase::UpdateCompositionText(const CompositionText& composition_,
                                            uint32_t cursor_pos,
                                            bool visible) {
  if (IsTextInputTypeNone())
    return;

  if (!SendFakeProcessKeyEvent(true)) {
    if (visible && !composition_.text.empty())
      GetTextInputClient()->SetCompositionText(composition_);
    else
      GetTextInputClient()->ClearCompositionText();
  }
  SendFakeProcessKeyEvent(false);
}

void InputMethodBase::DeleteSurroundingText(int32_t offset, uint32_t length) {}

void InputMethodBase::SendKeyEvent(KeyEvent* event) {
  sending_key_event_ = true;
  if (track_key_events_for_testing_) {
    key_events_for_testing_.push_back(
        std::unique_ptr<ui::KeyEvent>(new KeyEvent(*event)));
  }
  DispatchKeyEvent(event);
  sending_key_event_ = false;
}

InputMethod* InputMethodBase::GetInputMethod() {
  return this;
}

const std::vector<std::unique_ptr<ui::KeyEvent>>&
InputMethodBase::GetKeyEventsForTesting() {
  return key_events_for_testing_;
}

}  // namespace ui
