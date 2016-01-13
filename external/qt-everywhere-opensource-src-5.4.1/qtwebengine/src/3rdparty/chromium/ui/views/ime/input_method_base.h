// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_BASE_H_
#define UI_VIEWS_IME_INPUT_METHOD_BASE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/ime/input_method_delegate.h"

namespace gfx {
class Rect;
}

namespace ui {
class KeyEvent;
}

namespace views {

// A helper that provides functionality shared by InputMethod implementations.
class VIEWS_EXPORT InputMethodBase : public InputMethod,
                                     public FocusChangeListener {
 public:
  InputMethodBase();
  virtual ~InputMethodBase();

  // Overridden from InputMethod.
  virtual void SetDelegate(internal::InputMethodDelegate* delegate) OVERRIDE;
  virtual void Init(Widget* widget) OVERRIDE;
  virtual void OnTextInputTypeChanged(View* view) OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() const OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual bool IsMock() const OVERRIDE;

  // Overridden from FocusChangeListener.
  virtual void OnWillChangeFocus(View* focused_before, View* focused) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before, View* focused) OVERRIDE;

 protected:
  internal::InputMethodDelegate* delegate() const { return delegate_; }
  Widget* widget() const { return widget_; }
  View* GetFocusedView() const;

  // Returns true only if the View is focused and its Widget is active.
  bool IsViewFocused(View* view) const;

  // Returns true if there is no focused text input client or its type is none.
  bool IsTextInputTypeNone() const;

  // Calls the focused text input client's OnInputMethodChanged() method.
  // This has no effect if the text input type is ui::TEXT_INPUT_TYPE_NONE.
  void OnInputMethodChanged() const;

  // Convenience method to call delegate_->DispatchKeyEventPostIME().
  void DispatchKeyEventPostIME(const ui::KeyEvent& key) const;

  // Gets the current text input client's caret bounds in Widget's coordinates.
  // Returns false if the current text input client doesn't support text input.
  bool GetCaretBoundsInWidget(gfx::Rect* rect) const;

  // Removes any state installed on |widget_| and NULLs it out. Use if the
  // widget is in a state such that it should no longer be used (such as when
  // this is in its destructor).
  void DetachFromWidget();

 private:
  internal::InputMethodDelegate* delegate_;
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBase);
};

}  // namespace views

#endif  // UI_VIEWS_IME_INPUT_METHOD_BASE_H_
