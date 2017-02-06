// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/dummy_input_method.h"
#include "ui/events/event.h"

namespace ui {

DummyInputMethod::DummyInputMethod() {
}

DummyInputMethod::~DummyInputMethod() {
}

void DummyInputMethod::SetDelegate(internal::InputMethodDelegate* delegate) {
}

void DummyInputMethod::OnFocus() {
}

void DummyInputMethod::OnBlur() {
}

bool DummyInputMethod::OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                                NativeEventResult* result) {
  return false;
}

void DummyInputMethod::SetFocusedTextInputClient(TextInputClient* client) {
}

void DummyInputMethod::DetachTextInputClient(TextInputClient* client) {
}

TextInputClient* DummyInputMethod::GetTextInputClient() const {
  return NULL;
}

void DummyInputMethod::DispatchKeyEvent(ui::KeyEvent* event) {
}

void DummyInputMethod::OnTextInputTypeChanged(const TextInputClient* client) {
}

void DummyInputMethod::OnCaretBoundsChanged(const TextInputClient* client) {
}

void DummyInputMethod::CancelComposition(const TextInputClient* client) {
}

void DummyInputMethod::OnInputLocaleChanged() {
}

std::string DummyInputMethod::GetInputLocale() {
  return std::string();
}

TextInputType DummyInputMethod::GetTextInputType() const {
  return TEXT_INPUT_TYPE_NONE;
}

TextInputMode DummyInputMethod::GetTextInputMode() const {
  return TEXT_INPUT_MODE_DEFAULT;
}

int DummyInputMethod::GetTextInputFlags() const {
  return 0;
}

bool DummyInputMethod::CanComposeInline() const {
  return true;
}

bool DummyInputMethod::IsCandidatePopupOpen() const {
  return false;
}

void DummyInputMethod::ShowImeIfNeeded() {
}

void DummyInputMethod::AddObserver(InputMethodObserver* observer) {
}

void DummyInputMethod::RemoveObserver(InputMethodObserver* observer) {
}

const std::vector<std::unique_ptr<KeyEvent>>&
DummyInputMethod::GetKeyEventsForTesting() {
  return key_events_for_testing_;
}

}  // namespace ui
