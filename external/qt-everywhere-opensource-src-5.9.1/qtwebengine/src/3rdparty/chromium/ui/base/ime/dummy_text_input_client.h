// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_
#define UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "ui/base/ime/text_input_client.h"

namespace ui {

// Dummy implementation of TextInputClient. All functions do nothing.
class DummyTextInputClient : public TextInputClient {
 public:
  DummyTextInputClient();
  explicit DummyTextInputClient(TextInputType text_input_type);
  ~DummyTextInputClient() override;

  // Overriden from TextInputClient.
  void SetCompositionText(const CompositionText& composition) override;
  void ConfirmCompositionText() override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(const KeyEvent& event) override;
  TextInputType GetTextInputType() const override;
  TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetSelectionRange(gfx::Range* range) const override;
  bool SetSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretInRect(const gfx::Rect& rect) override;
  bool IsTextEditCommandEnabled(TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(TextEditCommand command) override;

  int insert_char_count() const { return insert_char_count_; }
  base::char16 last_insert_char() const { return last_insert_char_; }
  int insert_text_count() const { return insert_text_count_; }
  base::string16 last_insert_text() const { return last_insert_text_; }
  int set_composition_count() const { return set_composition_count_; }
  const CompositionText& last_composition() const { return last_composition_; }

  TextInputType text_input_type_;

  DISALLOW_COPY_AND_ASSIGN(DummyTextInputClient);

 private:
  int insert_char_count_;
  int insert_text_count_;
  int set_composition_count_;
  base::char16 last_insert_char_;
  base::string16 last_insert_text_;
  CompositionText last_composition_;
};

}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_
