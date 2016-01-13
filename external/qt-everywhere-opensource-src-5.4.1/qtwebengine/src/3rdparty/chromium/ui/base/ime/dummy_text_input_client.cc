// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/gfx/rect.h"

namespace ui {

DummyTextInputClient::DummyTextInputClient()
    : text_input_type_(TEXT_INPUT_TYPE_NONE) {}

DummyTextInputClient::DummyTextInputClient(TextInputType text_input_type)
    : text_input_type_(text_input_type) {}

DummyTextInputClient::~DummyTextInputClient() {
}

void DummyTextInputClient::SetCompositionText(
    const CompositionText& composition) {
}

void DummyTextInputClient::ConfirmCompositionText() {
}

void DummyTextInputClient::ClearCompositionText() {
}

void DummyTextInputClient::InsertText(const base::string16& text) {
}

void DummyTextInputClient::InsertChar(base::char16 ch, int flags) {
}

gfx::NativeWindow DummyTextInputClient::GetAttachedWindow() const {
  return NULL;
}

TextInputType DummyTextInputClient::GetTextInputType() const {
  return text_input_type_;
}

TextInputMode DummyTextInputClient::GetTextInputMode() const {
  return TEXT_INPUT_MODE_DEFAULT;
}

bool DummyTextInputClient::CanComposeInline() const {
  return false;
}

gfx::Rect DummyTextInputClient::GetCaretBounds() const {
  return gfx::Rect();
}

bool DummyTextInputClient::GetCompositionCharacterBounds(
    uint32 index,
    gfx::Rect* rect) const {
  return false;
}

bool DummyTextInputClient::HasCompositionText() const {
  return false;
}

bool DummyTextInputClient::GetTextRange(gfx::Range* range) const {
  return false;
}

bool DummyTextInputClient::GetCompositionTextRange(gfx::Range* range) const {
  return false;
}

bool DummyTextInputClient::GetSelectionRange(gfx::Range* range) const {
  return false;
}

bool DummyTextInputClient::SetSelectionRange(const gfx::Range& range) {
  return false;
}

bool DummyTextInputClient::DeleteRange(const gfx::Range& range) {
  return false;
}

bool DummyTextInputClient::GetTextFromRange(const gfx::Range& range,
                                            base::string16* text) const {
  return false;
}

void DummyTextInputClient::OnInputMethodChanged() {
}

bool DummyTextInputClient::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  return false;
}

void DummyTextInputClient::ExtendSelectionAndDelete(size_t before,
                                                    size_t after) {
}

void DummyTextInputClient::EnsureCaretInRect(const gfx::Rect& rect)  {
}

void DummyTextInputClient::OnCandidateWindowShown() {
}

void DummyTextInputClient::OnCandidateWindowUpdated() {
}

void DummyTextInputClient::OnCandidateWindowHidden() {
}

bool DummyTextInputClient::IsEditingCommandEnabled(int command_id) {
  return false;
}

void DummyTextInputClient::ExecuteEditingCommand(int command_id) {
}

}  // namespace ui
