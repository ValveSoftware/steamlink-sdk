// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_
#define UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_

#include "ui/base/ime/text_input_client.h"

namespace ui {

// Dummy implementation of TextInputClient. All functions do nothing.
class DummyTextInputClient : public TextInputClient {
 public:
  DummyTextInputClient();
  explicit DummyTextInputClient(TextInputType text_input_type);
  virtual ~DummyTextInputClient();

  // Overriden from TextInputClient.
  virtual void SetCompositionText(const CompositionText& composition) OVERRIDE;
  virtual void ConfirmCompositionText() OVERRIDE;
  virtual void ClearCompositionText() OVERRIDE;
  virtual void InsertText(const base::string16& text) OVERRIDE;
  virtual void InsertChar(base::char16 ch, int flags) OVERRIDE;
  virtual gfx::NativeWindow GetAttachedWindow() const OVERRIDE;
  virtual TextInputType GetTextInputType() const OVERRIDE;
  virtual TextInputMode GetTextInputMode() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;
  virtual gfx::Rect GetCaretBounds() const OVERRIDE;
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) const OVERRIDE;
  virtual bool HasCompositionText() const OVERRIDE;
  virtual bool GetTextRange(gfx::Range* range) const OVERRIDE;
  virtual bool GetCompositionTextRange(gfx::Range* range) const OVERRIDE;
  virtual bool GetSelectionRange(gfx::Range* range) const OVERRIDE;
  virtual bool SetSelectionRange(const gfx::Range& range) OVERRIDE;
  virtual bool DeleteRange(const gfx::Range& range) OVERRIDE;
  virtual bool GetTextFromRange(const gfx::Range& range,
                                base::string16* text) const OVERRIDE;
  virtual void OnInputMethodChanged() OVERRIDE;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE;
  virtual void ExtendSelectionAndDelete(size_t before, size_t after) OVERRIDE;
  virtual void EnsureCaretInRect(const gfx::Rect& rect) OVERRIDE;
  virtual void OnCandidateWindowShown() OVERRIDE;
  virtual void OnCandidateWindowUpdated() OVERRIDE;
  virtual void OnCandidateWindowHidden() OVERRIDE;
  virtual bool IsEditingCommandEnabled(int command_id) OVERRIDE;
  virtual void ExecuteEditingCommand(int command_id) OVERRIDE;

  TextInputType text_input_type_;

  DISALLOW_COPY_AND_ASSIGN(DummyTextInputClient);
};

}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_TEXT_INPUT_CLIENT_H_
