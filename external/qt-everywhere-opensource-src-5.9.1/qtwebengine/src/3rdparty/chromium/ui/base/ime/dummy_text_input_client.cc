// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

DummyTextInputClient::DummyTextInputClient()
    : text_input_type_(TEXT_INPUT_TYPE_NONE),
      insert_char_count_(0),
      insert_text_count_(0),
      set_composition_count_(0) {}

DummyTextInputClient::DummyTextInputClient(TextInputType text_input_type)
    : text_input_type_(text_input_type),
      insert_char_count_(0),
      insert_text_count_(0),
      set_composition_count_(0) {}

DummyTextInputClient::~DummyTextInputClient() {
}

void DummyTextInputClient::SetCompositionText(
    const CompositionText& composition) {
  ++set_composition_count_;
  last_composition_.CopyFrom(composition);
}

void DummyTextInputClient::ConfirmCompositionText() {
}

void DummyTextInputClient::ClearCompositionText() {
}

void DummyTextInputClient::InsertText(const base::string16& text) {
  ++insert_text_count_;
  last_insert_text_ = text;
}

void DummyTextInputClient::InsertChar(const KeyEvent& event) {
  ++insert_char_count_;
  last_insert_char_ = event.GetCharacter();
}

TextInputType DummyTextInputClient::GetTextInputType() const {
  return text_input_type_;
}

TextInputMode DummyTextInputClient::GetTextInputMode() const {
  return TEXT_INPUT_MODE_DEFAULT;
}

base::i18n::TextDirection DummyTextInputClient::GetTextDirection() const {
  return base::i18n::UNKNOWN_DIRECTION;
}

int DummyTextInputClient::GetTextInputFlags() const {
  return 0;
}

bool DummyTextInputClient::CanComposeInline() const {
  return false;
}

gfx::Rect DummyTextInputClient::GetCaretBounds() const {
  return gfx::Rect();
}

bool DummyTextInputClient::GetCompositionCharacterBounds(
    uint32_t index,
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

bool DummyTextInputClient::IsTextEditCommandEnabled(
    TextEditCommand command) const {
  return false;
}

void DummyTextInputClient::SetTextEditCommandForNextKeyEvent(
    TextEditCommand command) {}

}  // namespace ui
