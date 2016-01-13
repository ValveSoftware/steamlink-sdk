// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_BASE_H_
#define UI_BASE_IME_INPUT_METHOD_BASE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_base_export.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class InputMethodObserver;
class KeyEvent;
class TextInputClient;

// A helper class providing functionalities shared among ui::InputMethod
// implementations.
class UI_BASE_EXPORT InputMethodBase
   : NON_EXPORTED_BASE(public InputMethod),
     public base::SupportsWeakPtr<InputMethodBase> {
 public:
  InputMethodBase();
  virtual ~InputMethodBase();

  // Overriden from InputMethod.
  virtual void SetDelegate(internal::InputMethodDelegate* delegate) OVERRIDE;
  virtual void Init(bool focused) OVERRIDE;
  // If a derived class overrides OnFocus()/OnBlur(), it should call parent's
  // implementation first, to make sure |system_toplevel_window_focused_| flag
  // can be updated correctly.
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void SetFocusedTextInputClient(TextInputClient* client) OVERRIDE;
  virtual void DetachTextInputClient(TextInputClient* client) OVERRIDE;
  virtual TextInputClient* GetTextInputClient() const OVERRIDE;

  // If a derived class overrides this method, it should call parent's
  // implementation.
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE;

  virtual TextInputType GetTextInputType() const OVERRIDE;
  virtual TextInputMode GetTextInputMode() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;
  virtual void ShowImeIfNeeded() OVERRIDE;

  virtual void AddObserver(InputMethodObserver* observer) OVERRIDE;
  virtual void RemoveObserver(InputMethodObserver* observer) OVERRIDE;

 protected:
  virtual void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                         TextInputClient* focused) {}
  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) {}

  // Returns true if |client| is currently focused.
  bool IsTextInputClientFocused(const TextInputClient* client);

  // Checks if the focused text input client's text input type is
  // TEXT_INPUT_TYPE_NONE. Also returns true if there is no focused text
  // input client.
  bool IsTextInputTypeNone() const;

  // Convenience method to call the focused text input client's
  // OnInputMethodChanged() method. It'll only take effect if the current text
  // input type is not TEXT_INPUT_TYPE_NONE.
  void OnInputMethodChanged() const;

  // Convenience method to call delegate_->DispatchKeyEventPostIME().
  // Returns true if the event was processed
  bool DispatchKeyEventPostIME(const ui::KeyEvent& event) const;

  // Convenience method to notify all observers of TextInputClient changes.
  void NotifyTextInputStateChanged(const TextInputClient* client);

  // Interface for for signalling candidate window events.
  // See also *Callback functions below. To avoid reentrancy issue that
  // TextInputClient manipulates IME state during even handling, these methods
  // defer sending actual signals to renderer.
  void OnCandidateWindowShown();
  void OnCandidateWindowUpdated();
  void OnCandidateWindowHidden();

  bool system_toplevel_window_focused() const {
    return system_toplevel_window_focused_;
  }

 private:
  void SetFocusedTextInputClientInternal(TextInputClient* client);

  // Deferred callbacks for signalling TextInputClient about candidate window
  // appearance changes.
  void CandidateWindowShownCallback();
  void CandidateWindowUpdatedCallback();
  void CandidateWindowHiddenCallback();

  internal::InputMethodDelegate* delegate_;
  TextInputClient* text_input_client_;

  ObserverList<InputMethodObserver> observer_list_;

  bool system_toplevel_window_focused_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBase);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_BASE_H_
