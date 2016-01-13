// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_INPUT_METHOD_H_
#define UI_BASE_IME_DUMMY_INPUT_METHOD_H_

#include "ui/base/ime/input_method.h"

namespace ui {

class InputMethodObserver;

class DummyInputMethod : public InputMethod {
 public:
  DummyInputMethod();
  virtual ~DummyInputMethod();

  // InputMethod overrides:
  virtual void SetDelegate(
      internal::InputMethodDelegate* delegate) OVERRIDE;
  virtual void Init(bool focused) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual bool OnUntranslatedIMEMessage(
      const base::NativeEvent& event, NativeEventResult* result) OVERRIDE;
  virtual void SetFocusedTextInputClient(TextInputClient* client) OVERRIDE;
  virtual void DetachTextInputClient(TextInputClient* client) OVERRIDE;
  virtual TextInputClient* GetTextInputClient() const OVERRIDE;
  virtual bool DispatchKeyEvent(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE;
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE;
  virtual void CancelComposition(const TextInputClient* client) OVERRIDE;
  virtual void OnInputLocaleChanged() OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual bool IsActive() OVERRIDE;
  virtual TextInputType GetTextInputType() const OVERRIDE;
  virtual TextInputMode GetTextInputMode() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;
  virtual bool IsCandidatePopupOpen() const OVERRIDE;
  virtual void ShowImeIfNeeded() OVERRIDE;

  virtual void AddObserver(InputMethodObserver* observer) OVERRIDE;
  virtual void RemoveObserver(InputMethodObserver* observer) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyInputMethod);
};

}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_INPUT_METHOD_H_
