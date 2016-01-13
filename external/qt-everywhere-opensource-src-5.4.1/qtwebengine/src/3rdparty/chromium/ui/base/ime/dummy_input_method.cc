// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/dummy_input_method.h"

namespace ui {

DummyInputMethod::DummyInputMethod() {
}

DummyInputMethod::~DummyInputMethod() {
}

void DummyInputMethod::SetDelegate(internal::InputMethodDelegate* delegate) {
}

void DummyInputMethod::Init(bool focused) {
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

bool DummyInputMethod::DispatchKeyEvent(const ui::KeyEvent& event) {
  return false;
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

bool DummyInputMethod::IsActive() {
  return true;
}

TextInputType DummyInputMethod::GetTextInputType() const {
  return TEXT_INPUT_TYPE_NONE;
}

TextInputMode DummyInputMethod::GetTextInputMode() const {
  return TEXT_INPUT_MODE_DEFAULT;
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

}  // namespace ui
