// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_NULL_INPUT_METHOD_H_
#define UI_VIEWS_IME_NULL_INPUT_METHOD_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/ime/input_method.h"

namespace views {

// An implementation of views::InputMethod which does nothing.
//
// We're working on removing views::InputMethod{,Base,Bridge} and going to use
// only ui::InputMethod.  Use this class instead of views::InputMethodBridge
// with ui::TextInputFocusManager to effectively eliminate the
// views::InputMethod layer.
class NullInputMethod : public InputMethod {
 public:
  NullInputMethod();

  // Overridden from InputMethod:
  virtual void SetDelegate(internal::InputMethodDelegate* delegate) OVERRIDE;
  virtual void Init(Widget* widget) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) OVERRIDE;
  virtual void DispatchKeyEvent(const ui::KeyEvent& key) OVERRIDE;
  virtual void OnTextInputTypeChanged(View* view) OVERRIDE;
  virtual void OnCaretBoundsChanged(View* view) OVERRIDE;
  virtual void CancelComposition(View* view) OVERRIDE;
  virtual void OnInputLocaleChanged() OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual bool IsActive() OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() const OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual bool IsCandidatePopupOpen() const OVERRIDE;
  virtual void ShowImeIfNeeded() OVERRIDE;
  virtual bool IsMock() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullInputMethod);
};

}  // namespace views

#endif  // UI_VIEWS_IME_NULL_INPUT_METHOD_H_
